/* 
 * clilua.c ($Id$)
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 */


#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"


void setluaenv(lua_State *L);
int talklua(lua_State *L);
void setluamsgbuf(msgbuf_t *mb);

extern int typelua(lua_State *L);


/* Yuck! That slime mold must have been rotten! */
static msgbuf_t *msgbuf;


void setluamsgbuf(msgbuf_t *mb)
{
    msgbuf = mb;
    return;
}


void setluaenv(lua_State *L)
{
    lua_register(L, "setcuranim", setcuranimlua);
    lua_register(L, "talk", talklua);
    lua_register(L, "type", typelua);
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
    string_delete(&msgbuf->bottom->line);
    string_fromasciiz(&msgbuf->bottom->line, s);
    /* Note: this behavior might get annoying for people trying to
       read scrollback... consider a framing method instead. */
    msgbuf->current = msgbuf->current->next;
    msgbuf->bottom = msgbuf->bottom->next;
    if(msgbuf->bottom == msgbuf->head)
	msgbuf->head = msgbuf->head->next;
    return 0;
}

/* EOF clilua.c */
