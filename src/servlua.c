/* 
 * servlua.c ($Id$)
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 *
 * Server-specific Lua functions.
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


void setluaenv(lua_State *L);
int talklua(lua_State *L);


void setluaenv(lua_State *L)
{
    lua_register(L, "setcuranim", setcuranimlua);
    lua_register(L, "talk", talklua);
    lua_register(L, "type", typelua);
    lua_register(L, "tostring", tostringlua);
    lua_pushcfunction(L, setobjectlua);
    lua_settagmethod(L, LUAOBJECT_TAG, "settable");
    lua_pushcfunction(L, getobjectlua);
    lua_settagmethod(L, LUAOBJECT_TAG, "gettable");
}


int talklua(lua_State *L)
{
    const char *s;

    s = lua_tostring(L, -1);
    lua_pop(L, 1);
    d_error_debug("%s\n", s);
    return 0;
}

/* EOF servlua.c */
