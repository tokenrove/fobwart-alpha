/* 
 * data.c ($Id$)
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 *
 * Common file format and data structure support.
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
#include <dentata/random.h>
#include <dentata/util.h>

#include <lua.h>

#include "fobwart.h"


bool loadpalette(char *filename, d_palette_t *palette);
d_tilemap_t *loadtmap(char *filename);
d_sprite_t *loadsprite(char *filename);
bool loadscript(lua_State *L, char *filename);
bool initworldstate(worldstate_t *ws);
void destroyworldstate(worldstate_t *ws);
bool deskelobject(object_t *o);
bool deskelroom(room_t *room);

static void localsprintf(char *s, char *fmt, ...);


bool initworldstate(worldstate_t *ws)
{
    ws->objs = d_set_new(0);
    if(ws->objs == NULL) return failure;

    ws->rooms = d_set_new(0);
    if(ws->rooms == NULL) return failure;

    ws->luastate = lua_open(0);
    if(ws->luastate == NULL)
        return failure;

    return success;
}


void destroyworldstate(worldstate_t *ws)
{
    object_t *o;
    room_t *room;
    dword key;
    d_iterator_t it;

    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, ws->objs), key != D_SET_INVALIDKEY) {
        d_set_fetch(ws->objs, key, (void **)&o);
        d_sprite_delete(o->sprite);
        o->sprite = NULL;
    }
    d_set_delete(ws->objs);

    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, ws->rooms), key != D_SET_INVALIDKEY) {
        d_set_fetch(ws->rooms, key, (void **)&room);
        d_tilemap_delete(room->map);
        d_image_delete(room->bg);
        room->map = NULL;
    }
    d_set_delete(ws->rooms);

    lua_close(ws->luastate);
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


bool loadpalette(char *filename, d_palette_t *palette)
{
    d_file_t *file;
    dword i;

    file = d_file_open(filename);
    if(file == NULL)
        return failure;
    i = d_file_read(file, palette->clut, D_NCLUTITEMS*D_BYTESPERCOLOR);
    if(i != D_NCLUTITEMS*D_BYTESPERCOLOR)
        return failure;
    d_file_close(file);
    return success;
}


bool loadscript(lua_State *L, char *filename)
{
    int i;

    i = lua_dofile(L, filename);
    if(i != 0) {
        d_error_debug(__FUNCTION__": lua_dofile on %s failed with %d\n",
                      filename, i);
        return failure;
    }
    return success;
}


bool deskelobject(object_t *o)
{
    char *s;
    bool status;

    s = d_memory_new(strlen(SPRITEDATADIR)+strlen(o->spname)+6);
    localsprintf(s, "%s/%s.spr", SPRITEDATADIR, o->spname);
    o->sprite = loadsprite(s);
    if(o->sprite == NULL) {
        d_error_push(__FUNCTION__": sprite load failed.");
        return failure;
    }
    d_memory_delete(s);

    s = d_memory_new(strlen(SCRIPTDATADIR)+strlen(o->spname)+6);
    localsprintf(s, "%s/%s.luc", SCRIPTDATADIR, o->spname);

    o->luastate = lua_open(OBJSTACKSIZE);
    if(o->luastate == NULL) {
        d_error_push(__FUNCTION__": failed to init object's lua state.");
        return failure;
    }
    status = loadscript(o->luastate, DEFOBJSCRIPTFNAME);
    if(status != success) {
        d_error_push(__FUNCTION__": default script load failed.");
        return failure;
    }
    status = loadscript(o->luastate, s);
    if(status != success) {
        d_error_push(__FUNCTION__": unique script load failed.");
        return failure;
    }
    d_memory_delete(s);

    setluaenv(o->luastate);

    return success;
}


bool deskelroom(room_t *room)
{
    char *s, *t;
    dword key;
    d_iterator_t it;
    d_image_t *im;

    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, room->mapfiles), key != D_SET_INVALIDKEY) {
	d_set_fetch(room->mapfiles, key, (void **)&t);
	s = d_memory_new(strlen(TILEDATADIR)+strlen(t)+6);
	localsprintf(s, "%s/%s.pcx", TILEDATADIR, t);
	im = d_pcx_load(s);
	d_memory_delete(s);
	d_memory_copy(&room->map->tiledesc, &im->desc,
		      sizeof(d_rasterdescription_t));
	d_tilemap_addtileimage(room->map, key, im);
    }

    s = d_memory_new(strlen(BGDATADIR)+strlen(room->bgname)+6);
    localsprintf(s, "%s/%s.pcx", BGDATADIR, room->bgname);
    room->bg = d_pcx_load(s);
    d_memory_delete(s);
    if(room->bg == NULL) return failure;
    return success;
}


void localsprintf(char *s, char *fmt, ...)
{
    void **args;

    args = (void **)&fmt; args++;
    d_util_sprintf((byte *)s, (byte *)fmt, args);
    return;
}

/* EOF data.c */
