/* 
 * event.c
 * Created: Sun Jul 15 03:40:42 2001 by tek@wiw.org
 * Revised: Fri Jul 20 00:10:28 2001 by tek@wiw.org
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
#include <dentata/memory.h>
#include <dentata/s3m.h>
#include <dentata/set.h>
#include <dentata/random.h>
#include <dentata/util.h>

#include <lua.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <db.h>
#include <fcntl.h>

#include "fobwart.h"
#include "fobserv.h"


void processevents(serverdata_t *sd);


void processevents(serverdata_t *sd)
{
    event_t ev;
    object_t *o;
    room_t *room;
    bool status;
    int i;

    for(i = 0; i < sd->evsk.top; i++) {
        ev = sd->evsk.events[i];
        status = d_set_fetch(sd->ws.objs, ev.subject, (void **)&o);
        if(status != success) {
            d_error_debug(__FUNCTION__": failed to fetch object %d.\n",
                          ev.subject);
            continue;
        }

        status = d_set_fetch(sd->ws.rooms, o->location, (void **)&room);
        if(status != success) {
            d_error_debug(__FUNCTION__": failed to fetch current room!\n");
            continue;
        }

        switch(ev.verb) {
        case VERB_RIGHT:
            if(o->ax < XACCELMAX)
                o->ax++;
            d_sprite_setcuranim(o->sprite,
                                (o->onground)?ANIMRUNRIGHT:ANIMJUMPRIGHT);
            o->facing = right;
            break;

        case VERB_LEFT:
            if(o->ax > -XACCELMAX)
                o->ax--;
            d_sprite_setcuranim(o->sprite,
                                (o->onground)?ANIMRUNLEFT:ANIMJUMPLEFT);
            o->facing = left;
            break;

        case VERB_AUTO:
            o->ax = 0;
            d_sprite_setcuranim(o->sprite, (o->facing==left)?
                                ((o->onground)?ANIMSTANDLEFT:ANIMJUMPLEFT):
                                ((o->onground)?ANIMSTANDRIGHT:ANIMJUMPRIGHT));
            break;

        case VERB_ACT:
            break;

        case VERB_JUMP:
            if(o->onground) {
                o->ay = room->gravity;
                o->y--;
                o->vy = (-room->gravity)*JUMPVELOCITY;
                o->onground = false;
                d_sprite_setcuranim(o->sprite, (o->facing == left)?
                                    ANIMJUMPLEFT:ANIMJUMPRIGHT);
            }
            break;

        case VERB_TALK:
            fprintf(stderr, "<%s> %s\n", o->name, (char *)ev.auxdata);
            d_memory_delete(ev.auxdata);
            ev.auxdata = NULL;
            break;
        }
    }
    return;
}

/* EOF eventserv.c */
