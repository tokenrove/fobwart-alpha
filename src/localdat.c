/* 
 * localdat.c
 * Created: Sat Jul 14 23:40:37 2001 by tek@wiw.org
 * Revised: Thu Jul 19 18:38:33 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

#include <dentata/types.h>
#include <dentata/image.h>
#include <dentata/font.h>
#include <dentata/sprite.h>
#include <dentata/pcx.h>
#include <dentata/tilemap.h>
#include <dentata/manager.h>
#include <dentata/memory.h>
#include <dentata/file.h>
#include <dentata/raster.h>
#include <dentata/error.h>
#include <dentata/sample.h>
#include <dentata/audio.h>
#include <dentata/s3m.h>
#include <dentata/set.h>

#include <lua.h>

#include "fobwart.h"

extern d_image_t *ebar_new(gamedata_t *gd);

bool loaddata(gamedata_t *gd);
void destroydata(gamedata_t *gd);

bool loaddata(gamedata_t *gd)
{
    bool status;
    d_image_t *p;
    room_t *room;

    gd->deffont = d_font_load(DEFFONTFNAME);
    if(gd->deffont == NULL) return failure;
    gd->largefont = d_font_load(LARGEFONTFNAME);
    if(gd->largefont == NULL) return failure;

    gd->objs = d_set_new(0);
    if(gd->objs == NULL) return failure;

    gd->rooms = d_set_new(0);
    if(gd->rooms == NULL) return failure;
    gd->curroom = 0;
    room = d_memory_new(sizeof(room_t));
    if(room == NULL) return failure;
    status = d_set_add(gd->rooms, gd->curroom, (void *)room);
    if(status == failure) return failure;
    room->map = loadtmap(DATADIR "/rlevel.map");
    room->islit = true;
    room->gravity = 2;
    room->objs = gd->objs;

    /* load palette */
    loadpalette(LIGHTPALFNAME, &gd->light);
    loadpalette(DARKPALFNAME, &gd->dark);
    d_memory_copy(&gd->raster->palette, &gd->light,
                  D_NCLUTITEMS*D_BYTESPERCOLOR);

    status = d_manager_new();
    if(status == failure)
        return failure;

    /* FIXME note: memory leak here */
    p = d_pcx_load(DATADIR "/stars.pcx");
    if(p == NULL)
        return failure;
    d_manager_addimagelayer(p, NULL, -1);

    status = d_manager_addtilemaplayer(room->map, &room->tmhandle, 0);
    if(status == failure)
        return failure;

    d_manager_setscrollparameters(true, 0);

    gd->ebar = ebar_new(gd);

    if(gd->hasaudio) {
        gd->cursong = d_s3m_load(DATADIR "/mm2.s3m");
    }
    return success;
}


void destroydata(gamedata_t *gd)
{
    object_t *o;
    room_t *room;
    dword key;

    if(gd->hasaudio)
        d_s3m_delete(gd->cursong);

    d_image_delete(gd->ebar);
    d_manager_delete();

    d_set_resetiteration(gd->objs);
    while(key = d_set_nextkey(gd->objs), key != D_SET_INVALIDKEY) {
        d_set_fetch(gd->objs, key, (void **)&o);
        d_sprite_delete(o->sprite);
        o->sprite = NULL;
    }
    d_set_delete(gd->objs);

    d_set_resetiteration(gd->rooms);
    while(key = d_set_nextkey(gd->rooms), key != D_SET_INVALIDKEY) {
        d_set_fetch(gd->rooms, key, (void **)&room);
        d_tilemap_delete(room->map);
        room->map = NULL;
    }
    d_set_delete(gd->rooms);

    d_font_delete(gd->largefont);
    gd->largefont = NULL;
    d_font_delete(gd->deffont);
    gd->deffont = NULL;
    return;
}

/* EOF localdat.c */
