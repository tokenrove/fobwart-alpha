/* 
 * datacommon.c
 * Created: Thu Jul 19 18:35:37 2001 by tek@wiw.org
 * Revised: Fri Jul 20 04:05:38 2001 by tek@wiw.org
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


void loadpalette(char *filename, d_palette_t *palette);
d_tilemap_t *loadtmap(char *filename);
d_sprite_t *loadsprite(char *filename);
bool initworldstate(worldstate_t *ws);
void destroyworldstate(worldstate_t *ws);
void objencode(object_t *obj, DBT *data);
void objdecode(object_t *obj, DBT *data);
int bt_handle_cmp(DB *dbp, const DBT *a_, const DBT *b_);


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

    d_set_resetiteration(ws->objs);
    while(key = d_set_nextkey(ws->objs), key != D_SET_INVALIDKEY) {
        d_set_fetch(ws->objs, key, (void **)&o);
        d_sprite_delete(o->sprite);
        o->sprite = NULL;
    }
    d_set_delete(ws->objs);

    d_set_resetiteration(ws->rooms);
    while(key = d_set_nextkey(ws->rooms), key != D_SET_INVALIDKEY) {
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


void objencode(object_t *obj, DBT *data)
{
    int i;
    byte bytebuf;
    word wordbuf;

    data->size = strlen(obj->name)+1 + sizeof(roomhandle_t) +
                 6*sizeof(word) + 1 + 2*sizeof(word) +
                 strlen(obj->spname)+1;
    data->data = malloc(data->size);
    i = 0;
    memcpy(data->data, obj->name, strlen(obj->name)+1);
    i += strlen(obj->name)+1;
    wordbuf = htons(obj->location);
    memcpy(data->data+i, &wordbuf, sizeof(roomhandle_t));
    i += sizeof(roomhandle_t);
    wordbuf = htons(obj->x);
    memcpy(data->data+i, &wordbuf, sizeof(word));
    i += sizeof(word);
    wordbuf = htons(obj->y);
    memcpy(data->data+i, &wordbuf, sizeof(word));
    i += sizeof(word);
    wordbuf = htons(obj->ax);
    memcpy(data->data+i, &wordbuf, sizeof(word));
    i += sizeof(word);
    wordbuf = htons(obj->ay);
    memcpy(data->data+i, &wordbuf, sizeof(word));
    i += sizeof(word);
    wordbuf = htons(obj->vx);
    memcpy(data->data+i, &wordbuf, sizeof(word));
    i += sizeof(word);
    wordbuf = htons(obj->vy);
    memcpy(data->data+i, &wordbuf, sizeof(word));
    i += sizeof(word);
    bytebuf = 0;
    bytebuf |= (obj->onground == true) ? 1:0;
    bytebuf |= (obj->facing == left) ? 2:0;
    memcpy(data->data+i, &bytebuf, 1);
    i++;
    wordbuf = htons(obj->hp);
    memcpy(data->data+i, &wordbuf, sizeof(word));
    i += sizeof(word);
    wordbuf = htons(obj->maxhp);
    memcpy(data->data+i, &wordbuf, sizeof(word));
    i += sizeof(word);
    memcpy(data->data+i, obj->spname, strlen(obj->spname)+1);
    return;
}


void objdecode(object_t *obj, DBT *data)
{
    int i;
    byte bytebuf;
    word wordbuf;

    i = 0;
    obj->name = malloc(strlen(data->data)+1);
    memcpy(obj->name, data->data, strlen(data->data)+1);
    i += strlen(data->data)+1;
    memcpy(&wordbuf, data->data+i, sizeof(roomhandle_t));
    i += sizeof(roomhandle_t);
    obj->location = ntohs(wordbuf);
    memcpy(&wordbuf, data->data+i, sizeof(word));
    i += sizeof(word);
    obj->x = ntohs(wordbuf);
    memcpy(&wordbuf, data->data+i, sizeof(word));
    i += sizeof(word);
    obj->y = ntohs(wordbuf);
    memcpy(&wordbuf, data->data+i, sizeof(word));
    i += sizeof(word);
    obj->ax = ntohs(wordbuf);
    memcpy(&wordbuf, data->data+i, sizeof(word));
    i += sizeof(word);
    obj->ay = ntohs(wordbuf);
    memcpy(&wordbuf, data->data+i, sizeof(word));
    i += sizeof(word);
    obj->vx = ntohs(wordbuf);
    memcpy(&wordbuf, data->data+i, sizeof(word));
    i += sizeof(word);
    obj->vy = ntohs(wordbuf);
    memcpy(&bytebuf, data->data+i, 1);
    i++;
    obj->onground = (bytebuf&1) ? true:false;
    obj->facing = (bytebuf&2) ? left:right;
    memcpy(&wordbuf, data->data+i, sizeof(word));
    i += sizeof(word);
    obj->hp = ntohs(wordbuf);
    memcpy(&wordbuf, data->data+i, sizeof(word));
    i += sizeof(word);
    obj->maxhp = ntohs(wordbuf);
    obj->spname = malloc(strlen(data->data+i)+1);
    memcpy(obj->spname, data->data+i, strlen(data->data+i)+1);
    return;
}


int bt_handle_cmp(DB *dbp, const DBT *a_, const DBT *b_)
{
    word a, b;

    memcpy(&a, a_->data, sizeof(word));
    memcpy(&b, b_->data, sizeof(word));
    return (int)a - (int)b;
}

/* EOF datacommon.c */
