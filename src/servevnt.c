/* 
 * event.c
 * Created: Sun Jul 15 03:40:42 2001 by tek@wiw.org
 * Revised: Fri Jul 27 00:26:08 2001 by tek@wiw.org
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
#include "fobnet.h"
#include "fobdb.h"
#include "fobserv.h"


bool obtainobject(void *obdat_, objhandle_t handle, object_t **o);
bool obtainroom(void *obdat_, roomhandle_t handle, room_t **room);
void exitobject(void *obdat_, objhandle_t subject, roomhandle_t location);


bool obtainobject(void *obdat_, objhandle_t handle, object_t **o)
{
    bool status;
    serverdata_t *sd = obdat_;

    status = d_set_fetch(sd->ws.objs, handle, (void **)o);
    return status;
}


bool obtainroom(void *obdat_, roomhandle_t handle, room_t **room)
{
    bool status;
    serverdata_t *sd = obdat_;

    status = d_set_fetch(sd->ws.rooms, handle, (void **)room);
    return status;
}


void exitobject(void *obdat_, objhandle_t subject, roomhandle_t location)
{
    serverdata_t *sd = obdat_;
    object_t *o;
    room_t *room;
    d_image_t *im;

    d_set_fetch(sd->ws.objs, subject, (void **)&o);
    d_set_fetch(sd->ws.rooms, o->location, (void **)&room);
    d_set_remove(room->contents, subject);
    o->location = location;
    d_set_fetch(sd->ws.rooms, o->location, (void **)&room);
    d_set_add(room->contents, subject, NULL);
    im = d_sprite_getcurframe(o->sprite);

    if(o->y+im->desc.h >= room->map->h*room->map->tiledesc.h-1 &&
       o->vy > 0) {
	o->y = 0;

    } else if(o->y <= room->map->tiledesc.h && o->vy < 0) {
	o->y = room->map->h*room->map->tiledesc.h-im->desc.h-1;

    } else if(o->x+im->desc.w >= room->map->w*room->map->tiledesc.w-1 &&
	      o->vx > 0) {
	o->x = 0;

    } else if(o->x <= room->map->tiledesc.w && o->vx < 0) {
	o->x = room->map->w*room->map->tiledesc.w-im->desc.w-1;
    }
    return;
}

/* EOF eventserv.c */
