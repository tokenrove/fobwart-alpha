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

#include <stdio.h>
#include <errno.h>
#include <string.h>


bool net_sync(gamedata_t *gd);
bool handlepacket(gamedata_t *gd);
bool expectpacket(gamedata_t *gd, int waittype, packet_t *p);
static bool internalpackhandler(gamedata_t *gd, packet_t *p);
bool getobject(gamedata_t *gd, word handle);
bool getroom(gamedata_t *gd, word handle);
static bool savefile(char *name, byte *data, dword len);


bool net_sync(gamedata_t *gd)
{
    bool status;
    packet_t p;

    /* Send events */
    p.type = PACK_EVENT;
    while(evsk_pop(&gd->ws.evsk, &p.body.event))
        net_writepack(gd->nh, p);

    p.type = PACK_FRAME;
    status = net_writepack(gd->nh, p);
    if(status != success) {
        d_error_debug(__FUNCTION__": write frame failed.\n");
        return failure;
    }

    status = expectpacket(gd, PACK_FRAME, &p);
    if(status != success) {
        d_error_debug(__FUNCTION__": read frame failed.\n");
        return failure;
    }
    return success;
}


bool handlepacket(gamedata_t *gd)
{
    packet_t p;
    bool status;

    while(net_canread(gd->nh)) {
	status = net_readpack(gd->nh, &p);
	if(status != success)
	    return failure;

	status = internalpackhandler(gd, &p);
	if(status != success)
	    return failure;
    }

    return success;
}


/* expectpacket
 * Reads packets until either a packet of waittype is encounter, or
 * PACK_FAILURE. The last packet read is returned in p. Incidental packets
 * are handled transparently. */
bool expectpacket(gamedata_t *gd, int waittype, packet_t *p)
{
    bool status;

    do {
	status = net_readpack(gd->nh, p);
	if(status != success)
	    return failure;

	status = internalpackhandler(gd, p);
	if(status != success && p->type != waittype)
	    return failure;
    } while(p->type != waittype);

    return success;
}


/* internalpackhandler
 * Handles incidental packets. Returns success if the packet was handled,
 * or failure if it was an unknown type. */
bool internalpackhandler(gamedata_t *gd, packet_t *p)
{
    object_t *o;
    room_t *room;
    bool status;
    dword key, csum;
    d_iterator_t it;

    switch(p->type) {
    case PACK_EVENT:
	evsk_push(&gd->ws.evsk, p->body.event);
	break;


    case PACK_OBJECT:
	o = d_memory_new(sizeof(object_t));
	if(o == NULL) return failure;
	d_memory_copy(o, &p->body.object, sizeof(p->body.object));

	if(d_set_fetch(gd->ws.objs, o->handle, NULL))
	    d_set_remove(gd->ws.objs, o->handle);
	status = d_set_add(gd->ws.objs, o->handle, (void *)o);
	if(status != success) return failure;

	deskelobject(o);
	status = d_manager_addsprite(o->sprite, &o->sphandle, 0);
	if(status != success) {
	    d_error_push(__FUNCTION__": manager add sprite failed.");
	    return failure;
	}
	break;


    case PACK_ROOM:
	room = d_memory_new(sizeof(room_t));
	if(room == NULL) return failure;
	d_memory_copy(room, &p->body.room, sizeof(p->body.room));
	status = d_set_add(gd->ws.rooms, room->handle, (void *)room);
	if(status == failure) return failure;

	status = deskelroom(room);
	if(status == failure) return failure;
	d_memory_copy(&room->map->tiledesc, &room->map->tiles[0]->desc,
		      sizeof(room->map->tiledesc));

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

	d_error_debug(__FUNCTION__": Loading room contents\n");
	d_iterator_reset(&it);
	while(key = d_set_nextkey(&it, room->contents), key != D_SET_INVALIDKEY) {
	    d_error_debug(__FUNCTION__": Getting object %d\n", key);
	    if(getobject(gd, key) == failure) return failure;
	    d_error_debug(__FUNCTION__": Double checking fetch for object %d\n", key);
	    if(d_set_fetch(gd->ws.objs, key, (void **)&o) == failure)
		return failure;
	}
	d_error_debug(__FUNCTION__": Finished loading room contents\n");
	status = d_manager_addimagelayer(room->bg, &room->bghandle, -1);
	if(status == failure) return failure;

	status = d_manager_addtilemaplayer(room->map, &room->tmhandle, 0);
	if(status == failure) return failure;
	break;


    case PACK_FILE:
	if(savefile((char *)p->body.file.name.data, p->body.file.data,
		    p->body.file.length) != success)
	    return failure;

	csum = checksumfile((char *)p->body.file.name.data);
	if(csum != p->body.file.checksum) {
	    d_error_debug(__FUNCTION__": checksum failed on %s! "
			  "(%lx->%lx)\n", p->body.file.name.data,
			  p->body.file.checksum, csum);
	    return failure;
	}
	break;


    case PACK_FRAME:
    case PACK_RESLIST:
	break;

    default:
	d_error_debug(__FUNCTION__": received stray packet of type %d.\n",
		      p->type);
	return failure;
    }

    return success;
}


/* getobject
 * Requests and loads an object */
bool getobject(gamedata_t *gd, word handle)
{
    bool status;
    packet_t p;

    /* Send out a getobject packet, expect an object packet back. */
    d_error_debug("Added %d\n", handle);
    p.type = PACK_GETOBJECT;
    p.body.handle = handle;
    status = net_writepack(gd->nh, p);
    if(status != success) return failure;
    status = expectpacket(gd, PACK_OBJECT, &p);
    if(status != success || p.type != PACK_OBJECT) return failure;

    return success;
}


/* getroom
 * Requests and loads a room from the server. */
bool getroom(gamedata_t *gd, word handle)
{
    bool status;
    packet_t p;

    d_error_debug("Added room %d\n", handle);
    p.type = PACK_GETROOM;
    p.body.handle = handle;
    status = net_writepack(gd->nh, p);
    if(status == failure) return failure;
    status = expectpacket(gd, PACK_ROOM, &p);
    if(status == failure || p.type != PACK_ROOM) return failure;
    return success;
}


bool savefile(char *name, byte *data, dword len)
{
    FILE *fp;

    fp = fopen(name, "w");
    if(!fp) {
	fprintf(stderr, "Failed to open %s for writing: %s\n", name,
		strerror(errno));
	return failure;
    }

    if(fwrite(data, 1, len, fp) != len) {
	fprintf(stderr, "Failed to write %ld bytes into %s.\n", len,
		name);
	return failure;
    }

    fclose(fp);

    return success;
}


/* EOF clinet.c */
