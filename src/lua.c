/* 
 * lua.c ($Id$)
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 *
 * Common Lua interface routines and helper functions.
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


static worldstate_t *ws;

void setluaworldstate(worldstate_t *ws);
bool freezestate(lua_State *L, byte **data, dword *len);
bool meltstate(lua_State *L, char *data);

int setcuranimlua(lua_State *L);
int animhasloopedlua(lua_State *L);
int pushverblua(lua_State *L);
int setobjectlua(lua_State *L);
int getobjectlua(lua_State *L);
int typelua(lua_State *L);
int tostringlua(lua_State *L);


void setluaworldstate(worldstate_t *ws_)
{
    ws = ws_;
    return;
}


bool freezestate(lua_State *L, byte **data, dword *len)
{
    string_t str;
    int ret;

    *data = NULL;
    *len = 0;

    lua_getglobal(L, "freezestate");
    lua_getglobals(L);
    ret = lua_call(L, 1, 1);
    if(ret != 0) {
        d_error_debug(__FUNCTION__": call returned %d.\n", ret);
        return failure;
    }
    string_fromasciiz(&str, lua_tostring(L, -1));
    lua_pop(L, 1);
    *data = str.data;
    *len = str.len;
    return success;
}


bool meltstate(lua_State *L, char *data)
{
    int ret;

    ret = lua_dostring(L, data);
    if(ret != 0) {
        d_error_debug(__FUNCTION__": dostring returned %d.\n", ret);
        return failure;
    }
    return success;
}


int getobjectlua(lua_State *L)
{
    object_t *o;
    const char *s;

    o = lua_touserdata(L, -2);
    if(lua_isstring(L, -1)) {
        s = lua_tostring(L, -1);
        lua_pop(L, 2);
        if(strcmp(s, "name") == 0)
            lua_pushstring(L, o->name);
        else if(strcmp(s, "handle") == 0)
            lua_pushnumber(L, o->handle);
        else if(strcmp(s, "location") == 0)
            lua_pushnumber(L, o->location);
        else if(strcmp(s, "x") == 0)
            lua_pushnumber(L, o->x);
        else if(strcmp(s, "y") == 0)
            lua_pushnumber(L, o->y);
        else if(strcmp(s, "ax") == 0)
            lua_pushnumber(L, o->ax);
        else if(strcmp(s, "ay") == 0)
            lua_pushnumber(L, o->ay);
        else if(strcmp(s, "vx") == 0)
            lua_pushnumber(L, o->ay);
        else if(strcmp(s, "vy") == 0)
            lua_pushnumber(L, o->ay);
        else if(strcmp(s, "onground") == 0)
            lua_pushnumber(L, o->onground);
        else if(strcmp(s, "spname") == 0)
            lua_pushstring(L, o->spname);
        else if(strcmp(s, "sphandle") == 0)
            lua_pushnumber(L, o->sphandle);
        else
            lua_error(L, __FUNCTION__": index to object isn't valid");

    } else {
        lua_error(L, __FUNCTION__": index to object isn't a string");
    }
    return 1;
}


int setobjectlua(lua_State *L)
{
    object_t *o;
    const char *s;

    o = lua_touserdata(L, -3);
    if(lua_isstring(L, -2)) {
        s = lua_tostring(L, -2);
        if(strcmp(s, "name") == 0) {
            if(!lua_isstring(L, -1)) lua_error(L, __FUNCTION__": bad type");
            d_memory_delete(o->name);
            s = lua_tostring(L, -1);
            o->name = d_memory_new(strlen(s)+1);
            d_memory_copy(o->name, (char *)s, strlen(s)+1);

        } else if(strcmp(s, "handle") == 0) {
            if(!lua_isnumber(L, -1)) lua_error(L, __FUNCTION__": bad type");
            o->handle = lua_tonumber(L, -1);

        } else if(strcmp(s, "location") == 0) {
            if(!lua_isnumber(L, -1)) lua_error(L, __FUNCTION__": bad type");
            o->location = lua_tonumber(L, -1);

        } else if(strcmp(s, "x") == 0) {
            if(!lua_isnumber(L, -1)) lua_error(L, __FUNCTION__": bad type");
            o->x = lua_tonumber(L, -1);

        } else if(strcmp(s, "y") == 0) {
            if(!lua_isnumber(L, -1)) lua_error(L, __FUNCTION__": bad type");
            o->y = lua_tonumber(L, -1);

        } else if(strcmp(s, "ax") == 0) {
            if(!lua_isnumber(L, -1)) lua_error(L, __FUNCTION__": bad type");
            o->ax = lua_tonumber(L, -1);

        } else if(strcmp(s, "ay") == 0) {
            if(!lua_isnumber(L, -1)) lua_error(L, __FUNCTION__": bad type");
            o->ay = lua_tonumber(L, -1);

        } else if(strcmp(s, "vx") == 0) {
            if(!lua_isnumber(L, -1)) lua_error(L, __FUNCTION__": bad type");
            o->vx = lua_tonumber(L, -1);

        } else if(strcmp(s, "vy") == 0) {
            if(!lua_isnumber(L, -1)) lua_error(L, __FUNCTION__": bad type");
            o->vy = lua_tonumber(L, -1);

        } else if(strcmp(s, "onground") == 0) {
            if(!lua_isnumber(L, -1)) lua_error(L, __FUNCTION__": bad type");
            o->onground = (lua_tonumber(L, -1) > 0) ? true : false;

        } else if(strcmp(s, "spname") == 0) {
            if(!lua_isstring(L, -1)) lua_error(L, __FUNCTION__": bad type");
            d_memory_delete(o->spname);
            s = lua_tostring(L, -1);
            o->spname = d_memory_new(strlen(s)+1);
            d_memory_copy(o->spname, (char *)s, strlen(s)+1);

        } else if(strcmp(s, "sphandle") == 0) {
            if(!lua_isnumber(L, -1)) lua_error(L, __FUNCTION__": bad type");
            o->sphandle = lua_tonumber(L, -1);

        } else
            lua_error(L, __FUNCTION__": index to object isn't valid");

        lua_pop(L, 3);
    } else {
        lua_error(L, __FUNCTION__": index to object isn't a string");
    }
    return 0;
}


int setcuranimlua(lua_State *L)
{
    word anim;
    object_t *o;
    d_image_t *p;
    word oldh;

    o = lua_touserdata(L, 1);
    anim = lua_tonumber(L, 2);
    lua_pop(L, 2);

    p = d_sprite_getcurframe(o->sprite);
    oldh = p->desc.h;
    d_sprite_setcuranim(o->sprite, anim);
    p = d_sprite_getcurframe(o->sprite);
    if(oldh > p->desc.h)
        o->y += oldh-p->desc.h;
    else
        o->y -= p->desc.h-oldh;
    return 0;
}


int animhasloopedlua(lua_State *L)
{
    object_t *o;

    o = lua_touserdata(L, 1);
    lua_pop(L, 1);
    lua_pushnumber(L, d_sprite_haslooped(o->sprite));
    return 1;
}


int pushverblua(lua_State *L)
{
    event_t ev;
    word w;
    byte b;

    ev.subject = lua_tonumber(L, 1);
    ev.verb = lua_tonumber(L, 2);

    if(ev.verb == VERB_TALK) {
	ev.auxlen = lua_strlen(L, 3)+1;
	ev.auxdata = d_memory_new(ev.auxlen);
	d_memory_copy(ev.auxdata, lua_tostring(L, 3), ev.auxlen);
    } else if(ev.verb == VERB_ACT) {
	ev.auxlen = 1;
	ev.auxdata = d_memory_new(1);
	b = lua_tonumber(L, 3);
	d_memory_copy(&ev.auxdata, &b, 1);
    } else if(ev.verb == VERB_EXIT) {
	ev.auxlen = 2;
	ev.auxdata = d_memory_new(2);
	w = lua_tonumber(L, 3);
	d_memory_copy(&ev.auxdata, &w, 2);
    }
    evsk_push(&ws->evsk, ev);

    return 0;
}


int typelua(lua_State *L)
{
    int i;

    i = lua_type(L, -1);
    lua_pop(L, 1);
    lua_pushstring(L, lua_typename(L, i));
    return 1;
}


int tostringlua(lua_State *L)
{
    switch (lua_type(L, 1)) {
    case LUA_TNUMBER:
        lua_pushstring(L, lua_tostring(L, 1));
        return 1;
    case LUA_TSTRING:
        lua_pushvalue(L, 1);
        return 1;
    case LUA_TNIL:
        lua_pushstring(L, "nil");
        return 1;
    case LUA_TTABLE:
    case LUA_TFUNCTION:
    case LUA_TUSERDATA:
    default:
        lua_error(L, "value expected");
    }
    return 0;
}

/* EOF lua.c */
