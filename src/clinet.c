/* 
 * clinet.c ($Id$)
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 *
 * Client high-level network code.
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
#include <dentata/pcx.h>
#include <dentata/random.h>
#include <dentata/util.h>

#include <lua.h>

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"


bool getobject(gamedata_t *gd, word handle)
{
    bool status;
    object_t *o;
    packet_t p;

    d_error_debug("Added %d\n", handle);
    p.type = PACK_GETOBJECT;
    p.body.handle = handle;
    status = net_writepack(gd->nh, p);
    if(status == failure) return failure;
    status = net_readpack(gd->nh, &p);
    if(status == failure || p.type != PACK_OBJECT) return failure;

    o = d_memory_new(sizeof(object_t));
    if(o == NULL) return failure;
    status = d_set_add(gd->ws.objs, handle, (void *)o);
    if(status == failure) return failure;
    d_memory_copy(o, &p.body.object, sizeof(p.body.object));

    status = deskelobject(o);
    if(status == failure) return failure;

    meltstate(o->luastate, o->statebuf);
    d_memory_delete(o->statebuf);
    o->statebuf = NULL;

    status = d_manager_addsprite(o->sprite, &o->sphandle, 0);
    if(status == failure) {
        d_error_push(__FUNCTION__": manager add sprite failed.");
        return failure;
    }

    return status;
}


bool getroom(gamedata_t *gd, word handle)
{
    bool status;
    room_t *room;
    packet_t p;
    dword key;
    object_t *o;
    d_iterator_t it;

    d_error_debug("Added room %d\n", handle);
    p.type = PACK_GETROOM;
    p.body.handle = handle;
    status = net_writepack(gd->nh, p);
    if(status == failure) return failure;
    status = net_readpack(gd->nh, &p);
    if(status == failure || p.type != PACK_ROOM) return failure;

    room = d_memory_new(sizeof(room_t));
    if(room == NULL) return failure;
    status = d_set_add(gd->ws.rooms, handle, (void *)room);
    if(status == failure) return failure;
    d_memory_copy(room, &p.body.room, sizeof(p.body.room));

    /* deskelroom */
    status = deskelroom(room);
    if(status == failure) return failure;

    d_manager_wipelayers();
    d_manager_wipesprites();

    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, gd->ws.objs), key != D_SET_INVALIDKEY) {
        d_set_fetch(gd->ws.objs, key, (void **)&o);
        d_set_remove(gd->ws.objs, key);
        lua_close(o->luastate);
        d_sprite_delete(o->sprite);
        d_memory_delete(o);
        d_iterator_reset(&it);
    }

    d_error_debug("Loading room contents\n");
    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, room->contents), key != D_SET_INVALIDKEY) {
        d_error_debug("Getting object %d\n", key);
        if(getobject(gd, key) == failure) return failure;
        d_error_debug("Double checking fetch for object %d\n", key);
        if(d_set_fetch(gd->ws.objs, key, (void **)&o) == failure)
            return failure;
    }
    d_error_debug("Finished loading room contents\n");
    status = d_manager_addimagelayer(room->bg, &room->bghandle, -1);
    if(status == failure) return failure;

    status = d_manager_addtilemaplayer(room->map, &room->tmhandle, 0);
    if(status == failure) return failure;

    return success;
}


/* EOF clinet.c */
