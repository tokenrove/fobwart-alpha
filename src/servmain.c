/* 
 * fobserv.c
 * Created: Wed Jul 18 03:15:09 2001 by tek@wiw.org
 * Revised: Fri Jul 27 03:27:36 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

#include "fobwart.h"
#include "fobnet.h"
#include "fobdb.h"
#include "fobserv.h"

#include <dentata/error.h>
#include <dentata/color.h>
#include <dentata/memory.h>
#include <dentata/random.h>
#include <dentata/util.h>
#include <dentata/file.h>

#define PROGNAME "fobserv"


int handleclient(client_t *cli, serverdata_t *sd);
void wipeclient(d_set_t *clients, dword key);
void sendevents(serverdata_t *sd);
void unframe(serverdata_t *sd);
void mainloop(serverdata_t *sd);
void updatemanagedframes(worldstate_t *ws);
void checktransitions(serverdata_t *sd);


int main(void)
{
    serverdata_t sd;
    bool status;

    /* Initialize world state */
    status = initworldstate(&sd.ws);
    if(status != success)
        d_error_fatal("%s: failed to initize world.\n", PROGNAME);

    setluaworldstate(&sd.ws);

    /* Load the various databases */
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

    /* Load server data */
    status = loadservdata(&sd);
    if(status != success)
        d_error_fatal("%s: failed to load initial data.\n", PROGNAME);

    /* Empty client set, initialize the event stack */
    sd.clients = d_set_new(0);
    if(sd.clients == NULL)
        d_error_fatal("%s: unable to allocate client set.\n", PROGNAME);

    evsk_new(&sd.ws.evsk);

    /* Start listening on the network */
    if(net_newserver(&sd.nh, FOBPORT) == failure)
        d_error_fatal("%s: failed to initialize listening socket.\n",
                      PROGNAME);


    /* Do the main loop */
    mainloop(&sd);


    /* Shutdown and destroy */
    evsk_delete(&sd.ws.evsk);
    d_set_delete(sd.clients);
    closeobjectdb(sd.objectdb);
    deletedbh(sd.objectdb);
    closelogindb(sd.logindb);
    deletedbh(sd.logindb);
    destroyworldstate(&sd.ws);
    reslist_delete(&sd.reslist);

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
    activenalloc = 64; /* arbitrary */
    activeclients = d_memory_new(activenalloc*sizeof(dword));

    while(1) {
        net_servselect(sd->nh, &servsockactive, sd->clients, activeclients);

	/* handle new connections */
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
                evsk_push(&sd->ws.evsk, ev);
            }

	    checktransitions(sd);
            sendevents(sd);
            processevents(&sd->ws.evsk, (void *)sd);
            updatephysics(&sd->ws);
	    updatemanagedframes(&sd->ws);
            while(evsk_pop(&sd->ws.evsk, NULL));
	    unframe(sd);
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

    for(i = 0; i < sd->ws.evsk.top; i++) {
        ev = sd->ws.evsk.events[i];

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

    return;
}


void unframe(serverdata_t *sd)
{
    client_t *cli;
    dword key;
    packet_t p;
    d_iterator_t it;

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


int handleclient(client_t *cli, serverdata_t *sd)
{
    packet_t p;
    object_t *o;
    room_t *room;
    bool status;

    if(net_readpack(cli->nh, &p) == failure) return -1;

    switch(cli->state) {
    case 0: /* expect login or resource sync, all other packets rejected */
	switch(p.type) {
	case PACK_LOGIN:
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
	    break;

	case PACK_GETRESLIST:
	    p.type = PACK_RESLIST;
	    p.body.reslist = sd->reslist;
	    net_writepack(cli->nh, p);
	    break;

	case PACK_GETFILE:
            d_error_debug("%d requested file %s.\n", cli->handle,
			  p.body.string.data);
	    p.type = PACK_FILE;
	    p.body.file.name = p.body.string;
	    p.body.file.checksum = checksumfile((char *)p.body.file.name.data);
	    status = loadfile((char *)p.body.string.data, &p.body.file.data,
			      &p.body.file.length);
	    if(status == failure) {
		d_error_debug("Failed to send file %s to %d.\n",
			      p.body.string.data, cli->handle);
		return -1;
	    }
	    net_writepack(cli->nh, p);
	    break;

	default:
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
            evsk_push(&sd->ws.evsk, p.body.event);
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


/* wipeclient
 * Kick a client off the server. */
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


/* updatemanagedframes
 * Skip each sprite to its next frame, to avoid inconsistencies with
 * clients. */
void updatemanagedframes(worldstate_t *ws)
{
    object_t *o;
    dword key;
    d_iterator_t it;

    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, ws->objs), key != D_SET_INVALIDKEY) {
	d_set_fetch(ws->objs, key, (void **)&o);
	d_sprite_stepframe(o->sprite);
    }
    return;
}


void checktransitions(serverdata_t *sd)
{
    object_t *o;
    dword key;
    d_iterator_t it;
    d_image_t *im;
    room_t *room;
    event_t ev;

    ev.verb = VERB_EXIT;

    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, sd->ws.objs), key != D_SET_INVALIDKEY) {
	d_set_fetch(sd->ws.objs, key, (void **)&o);
	d_set_fetch(sd->ws.rooms, o->location, (void **)&room);

	ev.subject = key;
	im = d_sprite_getcurframe(o->sprite);

	if(o->y+im->desc.h >= room->map->h*room->map->tiledesc.h-1 &&
	   o->vy > 0) {
	    ev.auxdata = d_memory_new(sizeof(roomhandle_t));
	    d_memory_copy(ev.auxdata, &room->exits[2], sizeof(roomhandle_t));
	    ev.auxlen = sizeof(roomhandle_t);
            evsk_push(&sd->ws.evsk, ev);

	} else if(o->y <= room->map->tiledesc.h && o->vy < 0) {
	    ev.auxdata = d_memory_new(sizeof(roomhandle_t));
	    d_memory_copy(ev.auxdata, &room->exits[0], sizeof(roomhandle_t));
	    ev.auxlen = sizeof(roomhandle_t);
            evsk_push(&sd->ws.evsk, ev);

	} else if(o->x+im->desc.w >= room->map->w*room->map->tiledesc.w-1 &&
		  o->vx > 0) {
	    ev.auxdata = d_memory_new(sizeof(roomhandle_t));
	    d_memory_copy(ev.auxdata, &room->exits[1], sizeof(roomhandle_t));
	    ev.auxlen = sizeof(roomhandle_t);
            evsk_push(&sd->ws.evsk, ev);

	} else if(o->x <= room->map->tiledesc.w && o->vx < 0) {
	    ev.auxdata = d_memory_new(sizeof(roomhandle_t));
	    d_memory_copy(ev.auxdata, &room->exits[3], sizeof(roomhandle_t));
	    ev.auxlen = sizeof(roomhandle_t);
            evsk_push(&sd->ws.evsk, ev);
	}
    }
}

/* EOF fobserv.c */
