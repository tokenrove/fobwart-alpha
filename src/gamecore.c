/* 
 * gamecore.c ($Id$)
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 *
 * Shared physics and event processing.
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

#include "fobwart.h"


char *verbname[] = { "verb_nop", "verb_talk", "verb_right", "verb_left",
                     "verb_up", "verb_down", "verb_act", "verb_auto" };


void updatephysics(worldstate_t *ws);
bool checkpfcollide(d_sprite_t *sp, d_tilemap_t *tm, int x, int y);
void updateobjectphysics(worldstate_t *ws, object_t *obj);
void processevents(eventstack_t *evsk, void *obdat);


void processevents(eventstack_t *evsk, void *obdat)
{
    event_t ev;
    object_t *o;
    bool status;
    int i, ret, oldtop;
    word loc;

    for(i = 0; i < evsk->top; i++) {
        ev = evsk->events[i];
        status = obtainobject(obdat, ev.subject, &o);
        if(status != success) {
            d_error_debug(__FUNCTION__": failed to fetch object %d.\n",
                          ev.subject);
            continue;
        }

        switch(ev.verb) {
        case VERB_RIGHT:
        case VERB_LEFT:
        case VERB_UP:
        case VERB_DOWN:
        case VERB_AUTO:
            oldtop = lua_gettop(o->luastate);
            lua_getglobal(o->luastate, verbname[ev.verb]);
            lua_pushusertag(o->luastate, o, LUAOBJECT_TAG);
            ret = lua_call(o->luastate, 1, 0);
            if(ret != 0) {
                d_error_debug(__FUNCTION__": call %s for object %d returned "
                              "%d\n", verbname[ev.verb], ev.subject, ret);
            }
            lua_settop(o->luastate, oldtop);
            break;

        case VERB_ACT:
            oldtop = lua_gettop(o->luastate);
            lua_getglobal(o->luastate, verbname[ev.verb]);
            lua_pushusertag(o->luastate, o, LUAOBJECT_TAG);
            lua_pushnumber(o->luastate, ((byte *)ev.auxdata)[0]);
            ret = lua_call(o->luastate, 2, 0);
            if(ret != 0) {
                d_error_debug(__FUNCTION__": call %s for object %d returned "
                              "%d\n", verbname[ev.verb], ev.subject, ret);
            }
            lua_settop(o->luastate, oldtop);
            break;

        case VERB_TALK:
            oldtop = lua_gettop(o->luastate);
            lua_getglobal(o->luastate, verbname[ev.verb]);
            lua_pushusertag(o->luastate, o, LUAOBJECT_TAG);
            lua_pushstring(o->luastate, ev.auxdata);
            ret = lua_call(o->luastate, 2, 0);
            if(ret != 0) {
                d_error_debug(__FUNCTION__": call %s for object %d returned "
                              "%d\n", verbname[ev.verb], ev.subject, ret);
            }
            lua_settop(o->luastate, oldtop);
            break;

	case VERB_EXIT:
	    loc = *(roomhandle_t *)ev.auxdata;
	    exitobject(obdat, ev.subject, loc);
	    d_memory_delete(ev.auxdata);
	    break;
        }
    }

    return;
}


/* updatephysics
 * Integrates physics ahead one time step for each object. */
void updatephysics(worldstate_t *ws)
{
    object_t *o;
    dword key;
    d_iterator_t it;

    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, ws->objs), key != D_SET_INVALIDKEY) {
        d_set_fetch(ws->objs, key, (void **)&o);
        updateobjectphysics(ws, o);
    }
    return;
}


/* Physical constants */
enum { PHYS_HTERMINALV = 4, PHYS_VTERMINALV = 8 };

/* updateobjectphysics
 * Update an object's physical variables, move the object appropriately. */
void updateobjectphysics(worldstate_t *ws, object_t *obj)
{
    bool xdone, ydone;
    d_point_t dst, pt;
    int xinc, yinc;
    room_t *room;
    d_image_t *im;

    d_set_fetch(ws->rooms, obj->location, (void **)&room);

    /* Provide push algorithm, to avoid frame-differences causing
       havoc [FIXME] */


    /* Integrate acceleration into velocity */
    obj->vx += obj->ax;
    obj->vy += obj->ay;

    /* Apply terminal velocity rules */
    if(obj->vx > PHYS_HTERMINALV) obj->vx = PHYS_HTERMINALV;
    else if(obj->vx < -PHYS_HTERMINALV) obj->vx = -PHYS_HTERMINALV;

    if(obj->vy > PHYS_VTERMINALV) obj->vy = PHYS_VTERMINALV;
    else if(obj->vy < -PHYS_VTERMINALV) obj->vy = -PHYS_VTERMINALV;

    /* Integrate velocity into position */
    dst.x = obj->x+obj->vx;
    dst.y = obj->y+obj->vy;
    pt.x = obj->x; pt.y = obj->y;
    xdone = ydone = false;
    xinc = (obj->vx > 0) ? 1:-1;
    yinc = (obj->vy > 0) ? 1:-1;

    im = d_sprite_getcurframe(obj->sprite);

    while(!xdone || !ydone) {
	if(!xdone) {
	    if(pt.x == dst.x)
		xdone = true;
	    else
		pt.x += xinc;
	}
	if(!ydone) {
	    if(pt.y == dst.y)
		ydone = true;
	    else
		pt.y += yinc;
	}

	/* Boundry check */
	if(pt.y <= 0)
	    ydone = true;
	else if(pt.y+im->desc.h >= room->map->h*room->map->tiledesc.h)
	    ydone = true;

	if(pt.x <= 0)
	    xdone = true;
	else if(pt.x+im->desc.w >= room->map->w*room->map->tiledesc.w)
	    xdone = true;

	/* Progressive sprite-playfield collision checking */
	if(checkpfcollide(obj->sprite, room->map, pt.x, pt.y)) {
	  if(!ydone && checkpfcollide(obj->sprite, room->map,
				      pt.x-xinc, pt.y)) {
		pt.y -= yinc;
		ydone = true;
	    }
	    if(!xdone && checkpfcollide(obj->sprite, room->map,
					pt.x, pt.y-yinc)) {
		pt.x -= xinc;
		xdone = true;
	    }
	}
    }
    obj->x = pt.x;
    obj->y = pt.y;

    /* Check for grounding */
    if(obj->y >= room->map->h*room->map->tiledesc.h-1 ||
       checkpfcollide(obj->sprite, room->map, pt.x, pt.y+1)) {
	obj->onground = true;
    } else
	obj->onground = false;

    /* Apply friction */
    if(obj->vx > 0) obj->vx--;
    if(obj->vx < 0) obj->vx++;
}


bool checkpfcollide(d_sprite_t *sp, d_tilemap_t *tm, int x, int y)
{
    d_point_t pt, pt2;
    byte tile;
    d_image_t *im = d_sprite_getcurframe(sp);

    /* Check each tile's worth of the current frame. */
    for(pt.y = y; pt.y <= y+im->desc.h+tm->tiledesc.h-1;
	pt.y += tm->tiledesc.h) {

	for(pt.x = x; pt.x <= x+im->desc.w+tm->tiledesc.w-1;
	    pt.x += tm->tiledesc.w) {

	    /* If this tile is in the collidable range, go
	       on to further checks. */
	    tile = d_tilemap_gettile(tm, pt);
	    if(tile&128) {
		/* check pixel-perfect */
		pt2.x = x - (pt.x/tm->tiledesc.w)*tm->tiledesc.w;
		pt2.y = y - (pt.y/tm->tiledesc.h)*tm->tiledesc.h;
		if(d_image_collide(tm->tiles[tile], im, pt2, pixel))
		    return true;
	    }
	}
    }

    return false;
}

/* EOF gamecore.c */
