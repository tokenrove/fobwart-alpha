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
bool checktmcollide(d_image_t *p, d_tilemap_t *tm, d_point_t beg,
                    d_point_t end, byte *tile, d_point_t offs);
bool checkcollide(object_t *obj, d_tilemap_t *tm, worldstate_t *ws,
                  d_point_t pt, byte *ncollides, bool collided[4],
                  byte tile[4]);
bool checkobjcollide(worldstate_t *ws, object_t *o, d_point_t pt,
                     room_t *room, bool runlua);
void updateobjectphysics(worldstate_t *ws, object_t *obj);
void processevents(eventstack_t *evsk, void *obdat);
d_point_t nextpoint(int x, int y, int vx, int vy);
bool blockdirection(byte ncollides, bool collided[4], int vx, int vy,
                    bool *xdone, bool *ydone);


void processevents(eventstack_t *evsk, void *obdat)
{
    event_t ev;
    object_t *o;
    room_t *room;
    bool status;
    int i, ret, oldtop;

    for(i = 0; i < evsk->top; i++) {
        ev = evsk->events[i];
        status = obtainobject(obdat, ev.subject, &o);
        if(status != success) {
            d_error_debug(__FUNCTION__": failed to fetch object %d.\n",
                          ev.subject);
            continue;
        }

        status = obtainroom(obdat, o->location, &room);
        if(status != success) {
            d_error_debug(__FUNCTION__": failed to fetch current room!\n");
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
        }
    }

    return;
}


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


#define MAXATTEMPTS 1000

enum { upleft = 0, upright = 1, downleft = 2, downright = 3 };

/* Physical constants */
enum { TERMINALVELOCITY = 4 };

void updateobjectphysics(worldstate_t *ws, object_t *obj)
{
    room_t *curroom;
    bool status, xdone, ydone, collided[4];
    d_point_t pt, dst;
    byte ncollides, tile[4];

    status = d_set_fetch(ws->rooms, obj->location, (void **)&curroom);
    if(status == failure) {
        d_error_debug("Unable to obtain room %d!\n", obj->location);
        return;
    }

    /* Integrate acceleration into velocity. */
    obj->vx += obj->ax;
    if(obj->vx > TERMINALVELOCITY) obj->vx = TERMINALVELOCITY;
    else if(obj->vx < -TERMINALVELOCITY) obj->vx = -TERMINALVELOCITY;

    obj->vy += obj->ay;
    if(obj->vy > TERMINALVELOCITY) obj->vy = TERMINALVELOCITY;
    else if(obj->vy < -TERMINALVELOCITY) obj->vy = -TERMINALVELOCITY;

    /* Check for collision. */
    /* If collision occurred, do push algorithm. */

    xdone = (obj->vx == 0)?true:false;
    ydone = (obj->vy == 0)?true:false;

    dst.x = obj->x+obj->vx;
    dst.y = obj->y+obj->vy;

    /* While displacement update is not complete, */
    while(!xdone || !ydone) {
        /* pt = next point in parabola. */
        pt = nextpoint(obj->x, obj->y, xdone?0:obj->vx, ydone?0:obj->vy);
        /* Check for collision at pt. */
        status = checkcollide(obj, curroom->map, ws, pt, &ncollides, collided,
                              tile);
        /* If collision occurred, */
        if(status) {
            /* determine blocked direction. */
            status = blockdirection(ncollides, collided, obj->vx, obj->vy,
                                    &xdone, &ydone);
        }

        if(status == false) {
            /* set current position to pt. */
            obj->x = pt.x;
            obj->y = pt.y;
            if(obj->x == dst.x) xdone = true;
            if(obj->y == dst.y) ydone = true;
        }
    }

    /* --- */

    /* Get normal tile */
    pt.x = obj->x; pt.y = obj->y+1;
    status = checkcollide(obj, curroom->map, ws, pt, &ncollides, collided,
                          tile);
    if(status == true && (collided[downleft] || collided[downright])) {
        obj->ay = 0;
        obj->vy = 0;
        obj->onground = true;
    } else {
        obj->ay = curroom->gravity;
        obj->onground = false;
    }

    /* Sink horizontal velocity. */
    if(obj->vx != 0)
        obj->vx += (obj->vx > 0) ? -1 : 1;

    return;
}


d_point_t nextpoint(int x, int y, int vx, int vy)
{
    d_point_t pt;

    pt.x = x; pt.y = y;

    if(vx > 0) pt.x++;
    if(vx < 0) pt.x--;
    if(vy > 0) pt.y++;
    if(vy < 0) pt.y--;

    return pt;
}


bool blockdirection(byte ncollides, bool collided[4], int vx, int vy,
                    bool *xdone, bool *ydone)
{
    bool status;

    status = false;

    if(vx < 0 && (collided[upleft] || collided[downleft])) {
        *xdone = true;
        status = true;
    }
    if(vx > 0 && (collided[upright] || collided[downright])) {
        *xdone = true;
        status = true;
    }
    if(vy < 0 && (collided[upleft] || collided[upright])) {
        *ydone = true;
        status = true;
    }
    if(vy > 0 && (collided[downleft] || collided[downright])) {
        *ydone = true;
        status = true;
    }
    if(ncollides == 4) status = false;
    return status;
}


bool checkcollide(object_t *obj, d_tilemap_t *tm, worldstate_t *ws,
                  d_point_t pt, byte *ncollides, bool collided[4],
                  byte tile[4])
{
    d_image_t *p;
    d_point_t end, beg, half;
    bool status;

    p = d_sprite_getcurframe(obj->sprite);
    beg = end = half = pt;
    half.x += (p->desc.w+tm->tiledesc.w)/2;
    half.y += (p->desc.h+tm->tiledesc.h)/2;
    collided[0] = collided[1] = collided[2] = collided[3] = 0;
    tile[0] = tile[1] = tile[2] = tile[3] = 0;
    *ncollides = 0;

    end = half;
    status = checktmcollide(p, tm, beg, end, &tile[upleft], pt);
    if(status) {
        (*ncollides)++;
        collided[upleft] = true;
    }

    end.x = pt.x+p->desc.w+tm->tiledesc.w;
    beg.x = half.x+1;
    status = checktmcollide(p, tm, beg, end, &tile[upright], pt);
    if(status) {
        (*ncollides)++;
        collided[upright] = true;
    }

    end.x = half.x+1;
    end.y = pt.y+p->desc.h+tm->tiledesc.h;
    beg.x = pt.x;
    beg.y = half.y;
    status = checktmcollide(p, tm, beg, end, &tile[downleft], pt);
    if(status) {
        (*ncollides)++;
        collided[downleft] = true;
    }

    end.x = pt.x+p->desc.w+tm->tiledesc.w;
    beg.x = half.x+1;
    status = checktmcollide(p, tm, beg, end, &tile[downright], pt);
    if(status) {
        (*ncollides)++;
        collided[downright] = true;
    }

    return (*ncollides > 0) ? true : false;
}


bool checktmcollide(d_image_t *p, d_tilemap_t *tm, d_point_t beg,
                    d_point_t end, byte *tile, d_point_t offs)
{
    d_point_t pt;

    for(pt.y = beg.y; pt.y < end.y; pt.y+=tm->tiledesc.h) {
        for(pt.x = beg.x; pt.x < end.x; pt.x+=tm->tiledesc.w) {
            *tile = d_tilemap_gettile(tm, pt);
            if(*tile&128) {
                if(d_tilemap_checktilecollide(tm, p, pt, offs))
                    return true;
            }
        }
    }
    return false;
}


bool checkobjcollide(worldstate_t *ws, object_t *o, d_point_t pt, room_t *room,
                     bool runlua)
{
    d_iterator_t it;
    object_t *o2;
    d_point_t pt2;
    bool status;
    int oldtop, ret;
    dword key;

    status = false;

    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, room->contents), key != D_SET_INVALIDKEY) {
        if(key == o->handle) continue;
        d_set_fetch(ws->objs, key, (void **)&o2);
        pt2.x = o2->x-pt.x;
        pt2.y = o2->y-pt.y;
        if(d_image_collide(d_sprite_getcurframe(o->sprite),
                           d_sprite_getcurframe(o2->sprite),
                           pt2, pixel)) {
            if(runlua) {
                oldtop = lua_gettop(o->luastate);
                lua_getglobal(o->luastate, "objectcollide");
                lua_pushusertag(o->luastate, o, LUAOBJECT_TAG);
                lua_pushusertag(o->luastate, o2, LUAOBJECT_TAG);
                ret = lua_call(o->luastate, 2, 0);
                if(ret != 0) {
                    d_error_debug(__FUNCTION__": collide call for object %d "
                                  "returned %d\n", o->handle, ret);
                }
                lua_settop(o->luastate, oldtop);
            }

            status = true;
        }
    }
    return status;
}

/* EOF gamecore.c */
