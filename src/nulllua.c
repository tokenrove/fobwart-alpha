/*
 * nulllua.c
 */

#include "fobwart.h"

void setluaenv(lua_State *L)
{
    lua_register(L, "setcuranim", setcuranimlua);
    lua_register(L, "animhaslooped", animhasloopedlua);
    lua_register(L, "pushverb", pushverblua);
    /*    lua_register(L, "talk", talklua); */
    lua_register(L, "type", typelua);
    lua_register(L, "tostring", tostringlua);
    lua_pushcfunction(L, setobjectlua);
    lua_settagmethod(L, LUAOBJECT_TAG, "settable");
    lua_pushcfunction(L, getobjectlua);
    lua_settagmethod(L, LUAOBJECT_TAG, "gettable");
}

/* EOF nulllua.c */
