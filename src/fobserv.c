/* 
 * fobserv.c
 * Created: Wed Jul 18 03:15:09 2001 by tek@wiw.org
 * Revised: Fri Jul 20 06:17:03 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

#include <dentata/types.h>
#include <dentata/image.h>
#include <dentata/error.h>
#include <dentata/time.h>
#include <dentata/raster.h>
#include <dentata/event.h>
#include <dentata/font.h>
#include <dentata/sprite.h>
#include <dentata/tilemap.h>
#include <dentata/manager.h>
#include <dentata/sample.h>
#include <dentata/audio.h>
#include <dentata/s3m.h>
#include <dentata/set.h>
#include <dentata/color.h>
#include <dentata/memory.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <db.h>
#include <fcntl.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#include <lua.h>

#include "fobwart.h"
#include "fobserv.h"

#define PORT 6400
#define PROGNAME "fobserv"

int initlisten(int *sock);
int handleclient(client_t *cli, serverdata_t *sd);
int handshake(int clisock);
bool loadservdata(serverdata_t *sd);
void sendevents(serverdata_t *sd);
void mainloop(serverdata_t *sd);
bool loadlogindb(serverdata_t *sd);
void closelogindb(serverdata_t *sd);
bool verifylogin(serverdata_t *sd, char *name, char *pw, objhandle_t *object);
bool loadobjectdb(serverdata_t *sd);
void closeobjectdb(serverdata_t *sd);
bool getobject(serverdata_t *sd, objhandle_t key);


int main(void)
{
    serverdata_t sd;
    bool status;

    status = initworldstate(&sd.ws);
    if(status != success) {
        fprintf(stderr, "%s: failed to initize world.\n", PROGNAME);
        exit(EXIT_FAILURE);
    }

    status = loadlogindb(&sd);
    if(status != success) {
        fprintf(stderr, "%s: failed to load login.db.\n", PROGNAME);
        exit(EXIT_FAILURE);
    }

    status = loadobjectdb(&sd);
    if(status != success) {
        fprintf(stderr, "%s: failed to load object.db.\n", PROGNAME);
        exit(EXIT_FAILURE);
    }

    status = loadservdata(&sd);
    if(status != success) {
        fprintf(stderr, "%s: failed to load initial data.\n", PROGNAME);
        exit(EXIT_FAILURE);
    }

    if(initlisten(&sd.socket) == -1) {
        fprintf(stderr, "%s: failed to initialize listening socket.\n",
                PROGNAME);
        exit(EXIT_FAILURE);
    }

    sd.clients = d_set_new(0);
    if(sd.clients == NULL) {
        fprintf(stderr, "%s: unable to allocate client set.\n", PROGNAME);
        exit(EXIT_FAILURE);
    }

    evsk_new(&sd.evsk);

    mainloop(&sd);

    evsk_delete(&sd.evsk);
    d_set_delete(sd.clients);
    closeobjectdb(&sd);
    closelogindb(&sd);
    destroyworldstate(&sd.ws);

    exit(EXIT_SUCCESS);
}


void mainloop(serverdata_t *sd)
{
    int clisock;
    client_t *cli;
    int nfds, nframed, nloggedin, i;
    fd_set readfds, readfdbak;
    dword key;
    struct timeval timeout;
    bool status;

    nframed = 0;
    nloggedin = 0;

    while(1) {
        /* set readfds */
        FD_ZERO(&readfds);
        FD_SET(sd->socket, &readfds);
        nfds = sd->socket;

        d_set_resetiteration(sd->clients);
        while(key = d_set_nextkey(sd->clients), key != D_SET_INVALIDKEY) {
            d_set_fetch(sd->clients, key, (void **)&cli);
            if(cli->socket > nfds) nfds = cli->socket;
            FD_SET(cli->socket, &readfds);
        }

        d_memory_copy(&readfdbak, &readfds, sizeof(readfds));
        do {
            timeout.tv_sec = 0;
            timeout.tv_usec = 16000;
            d_memory_copy(&readfds, &readfdbak, sizeof(readfdbak));
        } while(select(nfds+1, &readfds, NULL, NULL, &timeout) == 0);

        if(FD_ISSET(sd->socket, &readfds)) {
            /* accept a new connection */
            clisock = accept(sd->socket, NULL, NULL);
            if(clisock == -1) {
                fprintf(stderr, "%s: accept: %s\n", PROGNAME, strerror(errno));
            } else if(handshake(clisock) == 0) {
                cli = d_memory_new(sizeof(client_t));
                /* note: cli->handle changes once the user logs in */
                cli->handle = d_set_getunusedkey(sd->clients);
                cli->socket = clisock;
                cli->state = 0;
                status = d_set_add(sd->clients, cli->handle, (void *)cli);
                if(status != success)
                    fprintf(stderr, "%s: d_set_add failed\n", PROGNAME);
            }
        }

        /* handle client requests */
        d_set_resetiteration(sd->clients);
        while(key = d_set_nextkey(sd->clients), key != D_SET_INVALIDKEY) {
            d_set_fetch(sd->clients, key, (void **)&cli);
            if(FD_ISSET(cli->socket, &readfds)) {
                i = handleclient(cli, sd);
                if(i == -1) {
                    close(cli->socket);
                    d_set_remove(sd->clients, key);
                }

                if(i == 1) {
                    nframed++;
                }
            }
        }

        /* sync frames */
        nloggedin = 0;
        d_set_resetiteration(sd->clients);
        while(key = d_set_nextkey(sd->clients), key != D_SET_INVALIDKEY) {
            d_set_fetch(sd->clients, key, (void **)&cli);
            if(cli->state > 0) nloggedin++;
        }

        if(nframed == nloggedin) {
            processevents(sd);
            updatephysics(&sd->ws);
            sendevents(sd);
            nframed = 0;
        }
    }
    return;
}


void sendevents(serverdata_t *sd)
{
    event_t ev;
    client_t *cli;
    dword key;
    packet_t p;
    object_t *o;
    roomhandle_t room;

    while(evsk_top(&sd->evsk, &ev)) {
        if(d_set_fetch(sd->ws.objs, ev.subject, (void **)&o))
            room = o->location;
        else {
            evsk_pop(&sd->evsk);
            continue;
        }

        d_set_resetiteration(sd->clients);
        while(key = d_set_nextkey(sd->clients), key != D_SET_INVALIDKEY) {
            d_set_fetch(sd->clients, key, (void **)&cli);
            d_set_fetch(sd->ws.objs, cli->handle, (void **)&o);
            if(cli->handle != ev.subject &&
               cli->state == 3 &&
               o->location == room) {
                p.type = PACK_EVENT;
                p.body.event = ev;
                writepack(cli->socket, p);
            }
        }
        evsk_pop(&sd->evsk);
    }

    d_set_resetiteration(sd->clients);
    while(key = d_set_nextkey(sd->clients), key != D_SET_INVALIDKEY) {
        d_set_fetch(sd->clients, key, (void **)&cli);
        if(cli->state != 3)
            continue;
        p.type = PACK_FRAME;
        writepack(cli->socket, p);
        cli->state = 2;
    }
    return;
}


int handshake(int clisock)
{
    packet_t p;

    p.type = PACK_YEAWHAW;
    p.body.handshake = 42;
    writepack(clisock, p);

    if(readpack(clisock, &p) == failure)
        return -1;
    if(p.type != PACK_IRECKON) {
        fprintf(stderr, "%s: bad handshake. moving on.", PROGNAME);
        close(clisock);
        return -1;
    }
    printf("handshake: %d\n", p.body.handshake);
    return 0;
}


int initlisten(int *sock)
{
    struct sockaddr_in addr;

    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if(*sock == -1)
        return -1;

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if(bind(*sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
        return -1;

    listen(*sock, 5);
    return 0;
}


bool loadservdata(serverdata_t *sd)
{
    dword key;
    room_t *room;
    bool status;
    char *s;

    /* load room db */
    sd->ws.rooms = d_set_new(0);
    if(sd->ws.rooms == NULL) return failure;

    room = d_memory_new(sizeof(room_t));
    if(room == NULL) return failure;
    room->handle = d_set_getunusedkey(sd->ws.rooms);
    status = d_set_add(sd->ws.rooms, room->handle, (void *)room);
    if(status == failure) return failure;
    room->name = "nowhere";
    room->islit = true;
    room->gravity = 2;
    room->mapname = "rlevel";
    s = d_memory_new(strlen(DATADIR)+strlen(room->mapname)+6);
    sprintf(s, "%s/%s.map", DATADIR, room->mapname);
    room->map = loadtmap(s);
    d_memory_delete(s);
    room->bgname = "stars";
    room->bg = NULL;
    room->contents = d_set_new(0);
    d_set_add(room->contents, 0, NULL);
    d_set_add(room->contents, 1, NULL);

    /* load object db */
    sd->ws.objs = d_set_new(0);
    if(sd->ws.objs == NULL) return failure;
    d_set_resetiteration(room->contents);
    while(key = d_set_nextkey(room->contents), key != D_SET_INVALIDKEY) {
        if(getobject(sd, key) != success)
            return failure;
    }
    return success;
}


int handleclient(client_t *cli, serverdata_t *sd)
{
    packet_t p;
    object_t *o;
    room_t *room;
    bool status;

    if(readpack(cli->socket, &p) == failure) return -1;

    switch(cli->state) {
    case 0: /* expect login, all other packets rejected */
        if(p.type == PACK_LOGIN) {
            fprintf(stderr, "Login attempt from %d (%s)", cli->handle,
                    p.body.login.name);
            status = verifylogin(sd, (char *)p.body.login.name,
                                 (char *)p.body.login.password,
                                 &p.body.handle);
            if(status == success) {
                fprintf(stderr, " succeeded.\n");
                p.type = PACK_AYUP;
                if(writepack(cli->socket, p) == failure) return -1;
                cli->handle = p.body.handle;
                cli->state++;
            } else {
                fprintf(stderr, " FAILED\n");
                return -1;
            }

        } else {
            fprintf(stderr, "%d tried to skip login phase.\n",
                    cli->handle);
            return -1;
        }
        break;

    case 1: /* expect sync */
    case 2: /* events */
        switch(p.type) {
        case PACK_EVENT:
            if(p.body.event.subject != cli->handle) {
                fprintf(stderr, "Got event from %d [calls itself %d]: %d + ``%s''\n",
                        cli->handle, p.body.event.subject, p.body.event.verb,
                        (p.body.event.auxlen)?(char *)p.body.event.auxdata:"");
                return -1;
            }
            evsk_push(&sd->evsk, p.body.event);
            break;

        case PACK_FRAME:
            cli->state = 3;
            return 1; /* return that we're framed */

        case PACK_GETOBJECT:
            status = d_set_fetch(sd->ws.objs, p.body.handle, (void **)&o);
            if(status == failure) {
                fprintf(stderr, "failed to fetch requested object %d\n",
                        p.body.handle);
                status = getobject(sd, p.body.handle);
                if(status == failure)
                    return -1;

                status = d_set_fetch(sd->ws.objs, p.body.handle, (void **)&o);
                if(status == failure)
                    return -1;
            }
            p.type = PACK_OBJECT;
            p.body.object = *o;
            writepack(cli->socket, p);
            break;

        case PACK_GETROOM:
            status = d_set_fetch(sd->ws.rooms, p.body.handle, (void **)&room);
            if(status == failure)
                return -1;
            p.type = PACK_ROOM;
            p.body.room = *room;
            writepack(cli->socket, p);
            break;
        }
        break;

    case 3: /* framed */
        fprintf(stderr, __FUNCTION__": %d broken! should be framed!\n",
                cli->handle);
        return -1;
    }

    return 0;
}


bool loadlogindb(serverdata_t *sd)
{
    int i;

    i = db_create(&sd->logindbp, NULL, 0);
    if(i != 0) {
        fprintf(stderr, "%s: db_create: %s\n", PROGNAME, db_strerror(i));
        return failure;
    }

    i = sd->logindbp->open(sd->logindbp, "login.db", NULL, DB_BTREE,
                           DB_CREATE, 0664);
    if(i != 0) {
        sd->logindbp->err(sd->logindbp, i, "%s", "login.db");
        return failure;
    }

    return success;
}


void closelogindb(serverdata_t *sd)
{
    sd->logindbp->close(sd->logindbp, 0);
    return;
}


bool verifylogin(serverdata_t *sd, char *name, char *pw, objhandle_t *object)
{
    DBT key, data;
    loginrec_t loginrec;
    word wordbuf;
    int i;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    memset(&loginrec, 0, sizeof(loginrec));

    key.data = name;
    key.size = strlen(name)+1;

    i = sd->logindbp->get(sd->logindbp, NULL, &key, &data, 0);
    if(i != 0)
        return failure;
    loginrec.password = data.data;
    memcpy(&wordbuf, (char *)data.data+strlen(loginrec.password)+1,
           sizeof(wordbuf));
    loginrec.object = ntohs(wordbuf);

    if(strcmp(loginrec.password, pw) != 0)
        return failure;

    *object = loginrec.object;
    return success;
}


bool loadobjectdb(serverdata_t *sd)
{
    int i;

    i = db_create(&sd->objectdbp, NULL, 0);
    if(i != 0) {
        fprintf(stderr, "%s: db_create: %s\n", PROGNAME, db_strerror(i));
        return failure;
    }

    i = sd->objectdbp->set_bt_compare(sd->objectdbp, bt_handle_cmp);
    if(i != 0) {
        sd->objectdbp->err(sd->objectdbp, i, "%s", "set_bt_compare");
        return failure;
    }

    i = sd->objectdbp->open(sd->objectdbp, "object.db", NULL, DB_BTREE,
                            DB_CREATE, 0664);
    if(i != 0) {
        sd->objectdbp->err(sd->objectdbp, i, "%s", "object.db");
        return failure;
    }

    return success;
}


void closeobjectdb(serverdata_t *sd)
{
    sd->objectdbp->close(sd->objectdbp, 0);
    return;
}


bool getobject(serverdata_t *sd, objhandle_t handle)
{
    object_t *o;
    DBT key, data;
    int i;
    char *s;
    bool status;

    key.size = sizeof(objhandle_t);
    key.data = malloc(key.size);
    memcpy(key.data, &handle, key.size);
    memset(&data, 0, sizeof(data));

    i = sd->objectdbp->get(sd->objectdbp, NULL, &key, &data, 0);
    if(i != 0) {
        sd->objectdbp->err(sd->objectdbp, i, "dbp->get");
        free(key.data);
        return failure;
    }

    o = d_memory_new(sizeof(object_t));
    if(o == NULL) return failure;
    o->handle = handle;
    status = d_set_add(sd->ws.objs, o->handle, (void *)o);
    if(status == failure) return failure;
    objdecode(o, &data);
    s = d_memory_new(strlen(DATADIR)+strlen(o->spname)+6);
    sprintf(s, "%s/%s.spr", DATADIR, o->spname);
    o->sprite = loadsprite(s);
    d_memory_delete(s);
    if(o->sprite == NULL) return failure;
    free(key.data);
    return success;
}

/* EOF fobserv.c */
