/* 
 * fobserv.c
 * Created: Wed Jul 18 03:15:09 2001 by tek@wiw.org
 * Revised: Fri Jul 27 03:27:36 2001 by tek@wiw.org
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
#include <dentata/random.h>
#include <dentata/util.h>

#include <lua.h>

#include "fobwart.h"
#include "fobnet.h"
#include "fobdb.h"
#include "fobserv.h"

#define PROGNAME "fobserv"

int handleclient(client_t *cli, serverdata_t *sd);
void wipeclient(d_set_t *clients, dword key);
bool loadservdata(serverdata_t *sd);
void sendevents(serverdata_t *sd);
void mainloop(serverdata_t *sd);
bool getobject(serverdata_t *sd, objhandle_t key);


int main(void)
{
    serverdata_t sd;
    bool status;

    status = initworldstate(&sd.ws);
    if(status != success)
        d_error_fatal("%s: failed to initize world.\n", PROGNAME);

    setluaworldstate(&sd.ws);

    sd.logindb = newdbh();
    status = loadlogindb(sd.logindb);
    if(status != success)
        d_error_fatal("%s: failed to load login db.\n", PROGNAME);

    sd.objectdb = newdbh();
    status = loadobjectdb(sd.objectdb);
    if(status != success)
        d_error_fatal("%s: failed to load object db.\n", PROGNAME);

    sd.roomdb = newdbh();
    status = loadroomdb(sd.roomdb);
    if(status != success)
        d_error_fatal("%s: failed to load room db.\n", PROGNAME);

    status = loadservdata(&sd);
    if(status != success)
        d_error_fatal("%s: failed to load initial data.\n", PROGNAME);

    if(net_newserver(&sd.nh, FOBPORT) == failure)
        d_error_fatal("%s: failed to initialize listening socket.\n",
                      PROGNAME);

    sd.clients = d_set_new(0);
    if(sd.clients == NULL)
        d_error_fatal("%s: unable to allocate client set.\n", PROGNAME);

    evsk_new(&sd.evsk);

    mainloop(&sd);

    evsk_delete(&sd.evsk);
    d_set_delete(sd.clients);
    closeobjectdb(sd.objectdb);
    deletedbh(sd.objectdb);
    closelogindb(sd.logindb);
    deletedbh(sd.logindb);
    destroyworldstate(&sd.ws);

    return 0;
}


void mainloop(serverdata_t *sd)
{
    nethandle_t *clinh;
    client_t *cli;
    dword key, *activeclients;
    bool status, servsockactive;
    event_t ev;
    d_iterator_t it;
    int nframed, nloggedin, activenalloc, i, ret;

    nframed = 0;
    nloggedin = 0;
    activenalloc = 64;
    activeclients = d_memory_new(activenalloc*sizeof(dword));

    while(1) {
        net_servselect(sd->nh, &servsockactive, sd->clients, activeclients);

        if(servsockactive) {
            net_accept(sd->nh, &clinh);
            cli = d_memory_new(sizeof(client_t));
            /* note: cli->handle changes once the user logs in */
            cli->handle = d_set_getunusedkey(sd->clients);
            cli->nh = clinh;
            cli->state = 0;
            status = d_set_add(sd->clients, cli->handle, (void *)cli);
            if(status != success)
                d_error_debug("%s: d_set_add failed\n", PROGNAME);

            if(d_set_nelements(sd->clients) >= activenalloc-1) {
                activenalloc *= 2;
                activeclients = d_memory_resize(activeclients,
                                                activenalloc*sizeof(dword));
            }
        }

        /* handle client requests */
        for(i = 0; activeclients[i] != D_SET_INVALIDKEY; i++) {
            d_set_fetch(sd->clients, activeclients[i], (void **)&cli);
            ret = handleclient(cli, sd);
            if(ret == -1)
                wipeclient(sd->clients, activeclients[i]);
            else if(ret == 1)
                nframed++;
        }

        /* sync frames */
        nloggedin = 0;
        d_iterator_reset(&it);
        while(key = d_set_nextkey(&it, sd->clients), key != D_SET_INVALIDKEY) {
            d_set_fetch(sd->clients, key, (void **)&cli);
            if(cli->state > 0) nloggedin++;
        }

        if(nframed == nloggedin) {
            /* deal with npcs, autos */
            d_iterator_reset(&it);
            while(key = d_set_nextkey(&it, sd->ws.objs),
                  key != D_SET_INVALIDKEY) {
                ev.subject = key;
                ev.verb = VERB_AUTO;
                ev.auxdata = NULL;
                ev.auxlen = 0;
                evsk_push(&sd->evsk, ev);
            }

            sendevents(sd);
            processevents(&sd->evsk, (void *)sd);
            updatephysics(&sd->ws);
            while(evsk_pop(&sd->evsk, NULL));
            nframed = 0;
        }
    }

    d_memory_delete(activeclients);
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
    d_iterator_t it;
    int i;

    for(i = 0; i < sd->evsk.top; i++) {
        ev = sd->evsk.events[i];

        if(d_set_fetch(sd->ws.objs, ev.subject, (void **)&o))
            room = o->location;
        else
            continue;

        d_iterator_reset(&it);
        while(key = d_set_nextkey(&it, sd->clients), key != D_SET_INVALIDKEY) {
            d_set_fetch(sd->clients, key, (void **)&cli);
            d_set_fetch(sd->ws.objs, cli->handle, (void **)&o);
            /* We don't send an event to a client who isn't framed.
             *
             * We don't send events to clients who aren't in the same
             * room as the event occurs in.
             */
            if(cli->state == 3 &&
               o->location == room) {
                p.type = PACK_EVENT;
                p.body.event = ev;
                if(net_writepack(cli->nh, p) != success)
                    wipeclient(sd->clients, key);
            }
        }
    }

    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, sd->clients), key != D_SET_INVALIDKEY) {
        d_set_fetch(sd->clients, key, (void **)&cli);
        if(cli->state != 3)
            continue;
        p.type = PACK_FRAME;
        if(net_writepack(cli->nh, p) != success)
            wipeclient(sd->clients, key);
        cli->state = 2;
    }
    return;
}


bool loadservdata(serverdata_t *sd)
{
    dword key, key2;
    room_t *room;
    object_t *o;
    bool status;
    d_iterator_t it, it2;

    /* load room db */
    sd->ws.rooms = d_set_new(0);
    if(sd->ws.rooms == NULL) return failure;
    
    room = d_memory_new(sizeof(room_t));
    if(room == NULL) return failure;
    room->handle = 0;
    status = d_set_add(sd->ws.rooms, room->handle, (void *)room);
    if(status == failure) return failure;

    /* The procedure for loading all rooms that will be here eventually
     * will either use a database cursor or will load a specified default
     * room and then traverse all connections from that room */
    status = roomdb_get(sd->roomdb, room->handle, room);
    if(status == failure) return failure;
    deskelroom(room);

    /* DUP FOR HACK */
    room = d_memory_new(sizeof(room_t));
    if(room == NULL) return failure;
    room->handle = 1;
    status = d_set_add(sd->ws.rooms, room->handle, (void *)room);
    if(status == failure) return failure;

    /* The procedure for loading all rooms that will be here eventually
     * will either use a database cursor or will load a specified default
     * room and then traverse all connections from that room */
    status = roomdb_get(sd->roomdb, room->handle, room);
    if(status == failure) return failure;
    deskelroom(room);
    /* END DUP */

    /* load object db */
    sd->ws.objs = d_set_new(0);
    if(sd->ws.objs == NULL) return failure;
    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, sd->ws.rooms), key != D_SET_INVALIDKEY) {
        d_set_fetch(sd->ws.rooms, key, (void **)&room);
        d_iterator_reset(&it2);
        while(key2 = d_set_nextkey(&it2, room->contents),
              key2 != D_SET_INVALIDKEY) {
            if(getobject(sd, key2) != success) {
                d_error_debug(__FUNCTION__": failed to get object %d\n", key2);
                return failure;
            }
            /* consistency checks */
            d_set_fetch(sd->ws.objs, key2, (void **)&o);
            if(o->location != key) {
                d_error_debug(__FUNCTION__": note: object %d had bad "
                              "location %d.\n", key2, o->location);
                o->location = key;
            }
        }
    }
    return success;
}


int handleclient(client_t *cli, serverdata_t *sd)
{
    packet_t p;
    object_t *o;
    room_t *room;
    bool status;

    if(net_readpack(cli->nh, &p) == failure) return -1;

    switch(cli->state) {
    case 0: /* expect login, all other packets rejected */
        if(p.type == PACK_LOGIN) {
            d_error_debug("Login attempt from %d (%s)", cli->handle,
                          p.body.login.name.data);
            status = verifylogin(sd->logindb, (char *)p.body.login.name.data,
                                 (char *)p.body.login.password.data,
                                 &p.body.handle);
            if(status == success) {
                d_error_debug(" succeeded.\n");
                p.type = PACK_AYUP;
                if(net_writepack(cli->nh, p) == failure) return -1;
                cli->handle = p.body.handle;
                cli->state++;
            } else {
                d_error_debug(" FAILED\n");
                return -1;
            }

        } else {
            d_error_debug("%d tried to skip login phase.\n", cli->handle);
            return -1;
        }
        break;

    case 1: /* expect sync */
    case 2: /* events */
        switch(p.type) {
        case PACK_EVENT:
            if(p.body.event.subject != cli->handle) {
                d_error_debug("Got event from %d [calls itself %d]: %d + "
                              "``%s''\n", cli->handle, p.body.event.subject,
                              p.body.event.verb, (p.body.event.auxlen)?
                              (char *)p.body.event.auxdata:"");
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
                d_error_debug("failed to fetch requested object %d\n",
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
            net_writepack(cli->nh, p);
            break;

        case PACK_GETROOM:
            status = d_set_fetch(sd->ws.rooms, p.body.handle, (void **)&room);
            if(status == failure)
                return -1;
            p.type = PACK_ROOM;
            p.body.room = *room;
            net_writepack(cli->nh, p);
            break;
        }
        break;

    case 3: /* framed */
        d_error_debug(__FUNCTION__": %d broken! should be framed!\n",
                      cli->handle);
        return -1;
    }

    return 0;
}


void wipeclient(d_set_t *clients, dword key)
{
    client_t *cli;
    bool status;

    status = d_set_fetch(clients, key, (void **)&cli);
    if(status != success) {
        d_error_debug(__FUNCTION__": bad key, couldn't fetch client.\n");
        return;
    }

    status = d_set_remove(clients, key);
    if(status != success) {
        d_error_debug(__FUNCTION__": couldn't remove client from set?\n");
        return;
    }

    net_close(cli->nh);
    cli->nh = NULL;
    cli->handle = -1;
    d_memory_delete(cli);
    return;
}


bool getobject(serverdata_t *sd, objhandle_t handle)
{
    object_t *o;
    bool status;

    o = d_memory_new(sizeof(object_t));
    if(o == NULL) return failure;
    o->handle = handle;
    objectdb_get(sd->objectdb, handle, o);
    status = d_set_add(sd->ws.objs, o->handle, (void *)o);
    if(status == failure) return failure;

    status = deskelobject(o);
    return status;
}


bool reloaddbs(serverdata_t *sd)
{
    bool status;

    closelogindb(sd->logindb);
    status = loadlogindb(sd->logindb);
    if(status != success) return failure;

    /* sync object db */
    closeobjectdb(sd->objectdb);
    status = loadobjectdb(sd->objectdb);
    if(status != success) return failure;

    return success;
}

/* EOF fobserv.c */
