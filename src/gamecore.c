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

/* Physical constants */
enum { XVELOCITYMAX = 4 };


void updatephysics(worldstate_t *ws);
bool checktmcollide(object_t *obj, d_tilemap_t *tm, d_point_t pt,
                    int vx, int vy);
bool checkobjcollide(worldstate_t *ws, object_t *o, d_point_t pt,
                     room_t *room, bool runlua);
void updateobjectphysics(worldstate_t *ws, object_t *obj);
void processevents(eventstack_t *evsk, void *obdat);


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


void updateobjectphysics(worldstate_t *ws, object_t *obj)
{
    d_point_t pt;
    room_t *curroom;
    bool status, groundcollide = false, goingdown = false;

    if(d_set_fetch(ws->rooms, obj->location, (void **)&curroom) == failure) {
        d_error_debug("Unable to obtain room %d!\n", obj->location);
        return;
    }
    checkobjcollide(ws, obj, pt, curroom, true);

    obj->vx += obj->ax;
    obj->vy += obj->ay;
    if(obj->vx > XVELOCITYMAX)
        obj->vx = XVELOCITYMAX;
    if(obj->vx < -XVELOCITYMAX)
        obj->vx = -XVELOCITYMAX;

    /* check collision, update position */
    pt.x = obj->x;
    pt.y = obj->y;

    while(pt.y != obj->y+obj->vy) {
        if(obj->vy > 0)
            goingdown = true;
        status = checktmcollide(obj, curroom->map, pt, 0, obj->vy);
        status |= checkobjcollide(ws, obj, pt, curroom, false);
        if(status == true) {
            if(goingdown)
                groundcollide = true;
            break;
        }
        if(pt.y != obj->y+obj->vy) {
            if(obj->vy > 0)
                pt.y++;
            else
                pt.y--;
        }
    }

    while(pt.x != obj->x+obj->vx) {
        status = checktmcollide(obj, curroom->map, pt, obj->vx, 0);
        status |= checkobjcollide(ws, obj, pt, curroom, false);
        if(status == true)
            break;
        if(pt.x != obj->x+obj->vx) {
            if(obj->vx > 0)
                pt.x++;
            else
                pt.x--;
        }
    }
    obj->x = pt.x;
    obj->y = pt.y;

    /* accel depends on whether we're on something solid or not */
    if(groundcollide || checktmcollide(obj, curroom->map, pt, 0, 1) == true) {
        obj->ay = 0;
        obj->onground = true;
    } else {
        obj->ay = curroom->gravity;
        obj->onground = false;
    }

    /* sink horizontal */
    if(obj->onground) {
        if(obj->vx > 0)
            obj->vx--;
        else if(obj->vx < 0)
            obj->vx++;
    }

    return;
}


bool checktmcollide(object_t *obj, d_tilemap_t *tm, d_point_t pt,
                    int vx, int vy)
{
    d_image_t *p;
    d_point_t pt2;

    p = d_sprite_getcurframe(obj->sprite);
    pt2 = pt;
    pt2.x += p->desc.w;
    pt2.y += p->desc.h;

    if(vx < 0) { /* left */
        pt.x--;
        for(pt.y++; pt.y < pt2.y; pt.y++)
            if(d_tilemap_gettile(tm, pt)&128)
                return true;
    }
    if(vx > 0) { /* right */
        pt.x += p->desc.w;
        for(pt.y++; pt.y < pt2.y; pt.y++) 
            if(d_tilemap_gettile(tm, pt)&128)
                return true;
    }
    if(vy < 0) { /* up */
        for(; pt.x < pt2.x; pt.x++)
            if(d_tilemap_gettile(tm, pt)&128)
                return true;
    }
    if(vy > 0) { /* down */
        pt.y += p->desc.h;
        for(; pt.x < pt2.x; pt.x++)
            if(d_tilemap_gettile(tm, pt)&128)
                return true;
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
