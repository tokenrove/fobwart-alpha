/* 
 * clievent.c ($Id$)
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
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

#include <sys/types.h>
#include <limits.h>
#include <db.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>

#include <lua.h>

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"


bool obtainobject(void *obdat_, objhandle_t handle, object_t **o);
bool obtainroom(void *obdat_, roomhandle_t handle, room_t **room);
void exitobject(void *gd_, objhandle_t subject, roomhandle_t location);


bool obtainobject(void *gd_, objhandle_t handle, object_t **o)
{
    bool status;
    gamedata_t *gd = gd_;

    status = d_set_fetch(gd->ws.objs, handle, (void **)o);
    if(status != success) {
        status = getobject(gd, handle);

        if(status != success) {
            d_error_debug(__FUNCTION__": failed to fetch object %d.\n",
                          handle);
            return failure;
        }

        status = d_set_fetch(gd->ws.objs, handle, (void **)o);
        if(status != success) {
            d_error_debug(__FUNCTION__": dwarf invasion object %d.\n",
                          handle);
            return failure;
        }
    }
    return success;
}


bool obtainroom(void *gd_, roomhandle_t handle, room_t **room)
{
    bool status;
    gamedata_t *gd = gd_;

    status = d_set_fetch(gd->ws.rooms, handle, (void **)room);
    if(status != success) {
        status = getroom(gd, handle);
        if(status != success) {
            d_error_debug(__FUNCTION__": failed to fetch room %d!\n", handle);
            return failure;
        }

        status = d_set_fetch(gd->ws.rooms, handle, (void **)room);
        if(status != success) {
            d_error_debug(__FUNCTION__": dwarf invasion room %d.\n",
                          handle);
            return failure;
        }
    }

    return success;
}


void exitobject(void *gd_, objhandle_t subject, roomhandle_t location)
{
    gamedata_t *gd = gd_;
    object_t *o;
    room_t *room;

    d_set_fetch(gd->ws.objs, subject, (void **)&o);
    d_set_fetch(gd->ws.rooms, o->location, (void **)&room);

    if(gd->localobj == subject) {
	/* If it's us, flush some things, get the new room. */
	d_tilemap_delete(room->map);
	d_image_delete(room->bg);
	d_set_remove(gd->ws.rooms, o->location);

	getroom(gd, location);
	d_set_fetch(gd->ws.rooms, location, (void **)&room);

    } else {
	/* If they're someone else, wax 'em when they leave. */
	d_manager_removesprite(o->sphandle);
	d_set_remove(gd->ws.objs, subject);
	d_set_remove(room->contents, subject);
        lua_close(o->luastate);
	d_sprite_delete(o->sprite);
	d_memory_delete(o);
    }
    return;
}

/* EOF clievent.c */
