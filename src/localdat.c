/* 
 * localdat.c
 * Created: Sat Jul 14 23:40:37 2001 by tek@wiw.org
 * Revised: Wed Jul 18 00:28:31 2001 by tek@wiw.org
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
d_sprite_t *loadsprite(char *fname);
d_tilemap_t *loadtmap(char *filename);
void loadpalette(char *filename, d_palette_t *palette);

bool loaddata(gamedata_t *gd)
{
    bool status;
    d_image_t *p;
    object_t *o;
    room_t *room;

    gd->deffont = d_font_load(DEFFONTFNAME);
    if(gd->deffont == NULL) return failure;
    gd->largefont = d_font_load(LARGEFONTFNAME);
    if(gd->largefont == NULL) return failure;

    gd->objs = d_set_new(0);
    if(gd->objs == NULL) return failure;
    gd->localobj = 0;
    o = d_memory_new(sizeof(object_t));
    if(o == NULL) return failure;
    status = d_set_add(gd->objs, gd->localobj, (void *)o);
    if(status == failure) return failure;
    o->sprite = loadsprite(DATADIR "/phibes.spr");
    o->name = "phibes";

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

    status = d_manager_addsprite(o->sprite, &o->sphandle, 0);
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

d_sprite_t *loadsprite(char *filename)
{
    d_sprite_t *spr;
    d_image_t *p;
    d_file_t *file;
    byte nanims, framelag, nframes;
    dword checksum;
    int i, j;
    d_rasterdescription_t mode;

    file = d_file_open(filename);
    if(file == NULL)
        return NULL;

    spr = d_sprite_new();
    if(spr == NULL)
        return NULL;

    nanims = d_file_getbyte(file);
    framelag = d_file_getbyte(file);
    checksum = d_file_getdword(file);
    for(i = 0; i < nanims; i++) {
        d_sprite_addanim(spr);
        nframes = d_file_getbyte(file);
        for(j = 0; j < nframes; j++) {
            mode.w = d_file_getword(file);
            mode.h = d_file_getword(file);
            mode.bpp = d_file_getbyte(file);
            mode.alpha = d_file_getbyte(file);
            mode.cspace = RGB;
            if(mode.bpp == 8)
                mode.paletted = true;
            else
                mode.paletted = false;

            p = d_image_new(mode);
            if(p == NULL)
                return NULL;

            d_file_read(file, p->data, mode.w*mode.h*(mode.bpp/8));
            if(mode.alpha > 0 && mode.bpp != 8)
                d_file_read(file, p->alpha, (mode.w*mode.h*mode.alpha+7)/8);

            d_sprite_addframe(spr, i, p);
        }
    }
    d_sprite_setanimparameters(spr, framelag);

    d_file_close(file);
    return spr;
}

d_tilemap_t *loadtmap(char *filename)
{
    d_tilemap_t *tm;
    d_file_t *file;
    d_image_t *p;
    word w, h;
    d_rasterdescription_t mode;
    dword checksum;
    int i;
    byte b;

    file = d_file_open(filename);
    if(file == NULL)
        return NULL;

    w = d_file_getword(file);
    h = d_file_getword(file);
    mode.w = d_file_getword(file);
    mode.h = d_file_getword(file);
    mode.bpp = d_file_getbyte(file);
    mode.alpha = d_file_getbyte(file);
    mode.cspace = RGB;
    if(mode.bpp == 8) mode.paletted = true;
    else mode.paletted = false;

    checksum = d_file_getdword(file);

    tm = d_tilemap_new(mode, w, h);
    if(tm == NULL)
        return NULL;

    d_file_read(file, tm->map, w*h);
    for(i = 0; i < 255; i++) {
        p = NULL;
        b = d_file_getbyte(file);
        if(b == 1) {
            p = d_image_new(mode);
            d_file_read(file, p->data, mode.w*mode.h*(mode.bpp/8));
            if(mode.alpha > 0 && mode.bpp != 8)
                d_file_read(file, p->alpha, (mode.w*mode.h*mode.bpp+7)/8);
        } else if(b != 0)
            d_error_fatal(__FUNCTION__": b was %d!\n", b);
        d_tilemap_addtileimage(tm, i, p);
    }
    return tm;
}

void loadpalette(char *filename, d_palette_t *palette)
{
    d_file_t *file;

    file = d_file_open(filename);
    if(file == NULL)
        return;
    d_file_read(file, palette->clut, D_NCLUTITEMS*D_BYTESPERCOLOR);
    d_file_close(file);
    return;
}

/* EOF localdat.c */
