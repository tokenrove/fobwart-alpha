/* 
 * physics.c
 * Created: Sun Jul 15 03:42:53 2001 by tek@wiw.org
 * Revised: Thu Jul 19 21:35:20 2001 by tek@wiw.org
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

#include <lua.h>

#include "fobwart.h"


void updatephysics(worldstate_t *ws);
bool checktmcollide(object_t *obj, d_tilemap_t *tm, d_point_t pt,
                    int vx, int vy);
void updateobjectphysics(worldstate_t *ws, object_t *obj);


void updatephysics(worldstate_t *ws)
{
    object_t *o;
    dword key;

    d_set_resetiteration(ws->objs);
    while(key = d_set_nextkey(ws->objs), key != D_SET_INVALIDKEY) {
        d_set_fetch(ws->objs, key, (void **)&o);
        updateobjectphysics(ws, o);
    }
}


void updateobjectphysics(worldstate_t *ws, object_t *obj)
{
    d_point_t pt;
    room_t *curroom;

    obj->vx += obj->ax;
    obj->vy += obj->ay;
    if(obj->vx > XVELOCITYMAX)
        obj->vx = XVELOCITYMAX;
    if(obj->vx < -XVELOCITYMAX)
        obj->vx = -XVELOCITYMAX;

    /* check collision, update position */
    pt.x = obj->x;
    pt.y = obj->y;

    d_set_fetch(ws->rooms, obj->location, (void **)&curroom);

    while(pt.y != obj->y+obj->vy) {
        if(checktmcollide(obj, curroom->map, pt, 0, obj->vy))
            break;
        if(pt.y != obj->y+obj->vy) {
            if(obj->vy > 0)
                pt.y++;
            else
                pt.y--;
        }
    }

    while(pt.x != obj->x+obj->vx) {
        if(checktmcollide(obj, curroom->map, pt, obj->vx, 0))
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
    if(checktmcollide(obj, curroom->map, pt, 0, 1) == true) {
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

/* EOF physics.c */
