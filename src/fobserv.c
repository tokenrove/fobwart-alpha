/* 
 * fobserv.c
 * Created: Wed Jul 18 03:15:09 2001 by tek@wiw.org
 * Revised: Thu Jul 19 20:45:43 2001 by tek@wiw.org
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
void sendevents(d_set_t *clients, eventstack_t *evsk);
bool loadservdata(serverdata_t *sd);

int main(void)
{
    int clisock;
    client_t *cli;
    int nfds, nframed, nloggedin, i;
    fd_set readfds, readfdbak;
    dword key;
    struct timeval timeout;
    bool status;
    serverdata_t sd;

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
    nframed = 0;

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(sd.socket, &readfds);
        nfds = sd.socket;
        d_set_resetiteration(sd.clients);
        while(key = d_set_nextkey(sd.clients), key != D_SET_INVALIDKEY) {
            d_set_fetch(sd.clients, key, (void **)&cli);
            if(cli->socket > nfds) nfds = cli->socket;
            FD_SET(cli->socket, &readfds);
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = 16000;

        d_memory_copy(&readfdbak, &readfds, sizeof(readfds));
        while(select(nfds+1, &readfds, NULL, NULL, &timeout) == 0) {
            timeout.tv_sec = 0;
            timeout.tv_usec = 16000;
            d_memory_copy(&readfds, &readfdbak, sizeof(readfdbak));
        }

        if(FD_ISSET(sd.socket, &readfds)) {
            clisock = accept(sd.socket, NULL, NULL);
            if(clisock == -1) {
                fprintf(stderr, "%s: accept: %s\n", PROGNAME, strerror(errno));
            } else if(handshake(clisock) == 0) {
                cli = d_memory_new(sizeof(client_t));
                /* note: cli->handle changes once the user logs in */
                cli->handle = d_set_getunusedkey(sd.clients);
                cli->socket = clisock;
                cli->state = 0;
                status = d_set_add(sd.clients, cli->handle, (void *)cli);
                if(status != success)
                    fprintf(stderr, "%s: d_set_add failed\n", PROGNAME);
            }
        }

        nloggedin = 0;
        d_set_resetiteration(sd.clients);
        while(key = d_set_nextkey(sd.clients), key != D_SET_INVALIDKEY) {
            d_set_fetch(sd.clients, key, (void **)&cli);
            if(FD_ISSET(cli->socket, &readfds)) {
                i = handleclient(cli, &sd);
                if(i == -1) {
                    close(cli->socket);
                    d_set_remove(sd.clients, key);
                } else if(cli->state > 0)
                    nloggedin++;

                if(i == 1) {
                    nframed++;
                }
            }
        }

        if(nframed == nloggedin) {
            sendevents(sd.clients, &sd.evsk);
            nframed = 0;
        }
    }

    evsk_delete(&sd.evsk);
    d_set_delete(sd.clients);

    exit(EXIT_SUCCESS);
}


void sendevents(d_set_t *clients, eventstack_t *evsk)
{
    event_t ev;
    client_t *cli;
    dword key;
    packet_t p;

    while(evsk_top(evsk, &ev)) {
        d_set_resetiteration(clients);
        while(key = d_set_nextkey(clients), key != D_SET_INVALIDKEY) {
            d_set_fetch(clients, key, (void **)&cli);
            if(cli->handle != ev.subject && cli->state == 3) {
                p.type = PACK_EVENT;
                p.body.event = ev;
                writepack(cli->socket, p);
            }
        }
        evsk_pop(evsk);
    }

    d_set_resetiteration(clients);
    while(key = d_set_nextkey(clients), key != D_SET_INVALIDKEY) {
        d_set_fetch(clients, key, (void **)&cli);
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
    object_t *o;
    bool status;

    /* load login db */
    /* load object db */
    sd->objs = d_set_new(0);
    if(sd->objs == NULL) return failure;
    o = d_memory_new(sizeof(object_t));
    if(o == NULL) return failure;
    status = d_set_add(sd->objs, 0, (void *)o);
    if(status == failure) return failure;
    o->sprite = loadsprite(DATADIR "/phibes.spr");
    o->name = "phibes";
    o->ax = o->vx = o->vy = 0;
/*    o->ay = room->gravity; */
    o->ay = 2;
    o->onground = false;
    o->x = 64;
    o->y = 64;
    o->maxhp = 412;
    o->hp = 200;

    o = d_memory_new(sizeof(object_t));
    if(o == NULL) return failure;
    status = d_set_add(sd->objs, 1, (void *)o);
    if(status == failure) return failure;
    o->sprite = loadsprite(DATADIR "/phibes.spr");
    o->name = "STAN";
    o->ax = o->vx = o->vy = 0;
/*    o->ay = room->gravity; */
    o->ay = 2;
    o->onground = false;
    o->x = 64;
    o->y = 64;
    o->maxhp = 412;
    o->hp = 200;
    
    /* load room db */
    return success;
}


int handleclient(client_t *cli, serverdata_t *sd)
{
    packet_t p;
    object_t *o;
    bool status;

    if(readpack(cli->socket, &p) == failure) return -1;

    switch(cli->state) {
    case 0: /* expect login, all other packets rejected */
        if(p.type == PACK_LOGIN) {
            fprintf(stderr, "Login from %d (%s/%s)\n", cli->handle,
                    p.body.login.name, p.body.login.password);
            p.type = PACK_AYUP;
            p.body.handle = cli->handle;
            if(writepack(cli->socket, p) == failure) return -1;
            cli->state++;

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
                        (p.body.event.auxlen)?p.body.event.auxdata:"");
                return -1;
            }
            evsk_push(&sd->evsk, p.body.event);
            break;

        case PACK_FRAME:
            cli->state = 3;
            return 1; /* return that we're framed */

        case PACK_GETOBJECT:
            status = d_set_fetch(sd->objs, p.body.handle, (void **)&o);
            if(status == failure)
                return -1;
            p.type = PACK_OBJECT;
            p.body.object = *o;
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

/* EOF fobserv.c */
