/* 
 * fobwart.h
 * Created: Sat Jul 14 23:23:21 2001 by tek@wiw.org
 * Revised: Fri Jul 27 00:23:42 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */


#include <dentata/types.h>
#include <dentata/image.h>
#include <dentata/sprite.h>
#include <dentata/tilemap.h>
#include <dentata/set.h>

#include <lua.h>


#define DATADIR "./data"
#define BGDATADIR      DATADIR "/backgrounds"
#define TILEDATADIR    DATADIR "/tiles"
#define SPRITEDATADIR  DATADIR "/sprites"
#define SCRIPTDATADIR  DATADIR "/scripts"
#define FONTDATADIR    DATADIR "/fonts"
#define PALETTEDATADIR DATADIR "/palettes"
#define DEFFONTFNAME   FONTDATADIR "/def.fnt"
#define LARGEFONTFNAME FONTDATADIR "/slant.fnt"
#define LIGHTPALFNAME  PALETTEDATADIR "/light.pal"
#define DARKPALFNAME   PALETTEDATADIR "/dark.pal"
#define DEFOBJSCRIPTFNAME SCRIPTDATADIR "/defobj.luc"

enum { FOBPORT = 6400 };

/* Event verbs */
enum {
    VERB_NOP = 0, VERB_TALK, VERB_RIGHT, VERB_LEFT, VERB_UP, VERB_DOWN,
    VERB_ACT, VERB_AUTO, VERB_EXIT
};

/* Other game constants */
enum { OBJSTACKSIZE = 0, LUAOBJECT_TAG = 0, NEXITS_PER_ROOM = 4 };

typedef word objhandle_t;
typedef word roomhandle_t;


typedef struct string_s {
    byte *data;
    dword len;
} string_t;


typedef struct typebuf_s {
    byte *buf;
    word pos, nalloc;
    bool done;
} typebuf_t;


typedef struct object_s {
    char *name;
    objhandle_t handle;
    roomhandle_t location;

    /* physics */
    word x, y;
    short ax, ay;
    short vx, vy;
    bool onground;

    /* graphics */
    char *spname;
    d_sprite_t *sprite;
    word sphandle;

    /* scripts */
    lua_State *luastate;
} object_t;


typedef struct room_s {
    char *name, *owner;
    roomhandle_t handle;

    /* physics */
    int gravity;
    bool islit;

    /* graphics */
    d_set_t *mapfiles;
    d_tilemap_t *map;
    word tmhandle;

    char *bgname;
    d_image_t *bg;
    word bghandle;

    /* contents */
    d_set_t *contents;

    roomhandle_t exits[NEXITS_PER_ROOM];

    /* scripts */
    lua_State *luastate;
} room_t;


typedef struct event_s {
    word subject;
    byte verb;
    void *auxdata;
    dword auxlen;
} event_t;


typedef struct eventstack_s {
    int top, nalloc;
    event_t *events;
} eventstack_t;


typedef struct worldstate_s {
    d_set_t *objs;
    d_set_t *rooms;
    eventstack_t evsk;
    lua_State *luastate;
} worldstate_t;


typedef struct reslist_s {
    word length;
    string_t *name;
    dword *checksum;
} reslist_t;


/* util.c */
extern void evsk_new(eventstack_t *evsk);
extern void evsk_delete(eventstack_t *evsk);
extern void evsk_push(eventstack_t *evsk, event_t ev);
extern bool evsk_top(eventstack_t *evsk, event_t *ev);
extern bool evsk_pop(eventstack_t *evsk, event_t *ev);

/* util.c */
extern bool string_fromasciiz(string_t *dst, const char *src);
extern void string_delete(string_t *s);

/* util.c */
extern dword checksumfile(const char *name);
extern void checksuminit(void);

/* util.c */
extern void reslist_delete(reslist_t *reslist);
extern void reslist_add(reslist_t *reslist, char *dir, char *name, char *ext);

/* data.c */
extern d_sprite_t *loadsprite(char *fname);
extern d_tilemap_t *loadtmap(char *filename);
extern bool loadpalette(char *filename, d_palette_t *palette);
extern bool loadscript(lua_State *L, char *filename);
extern bool deskelobject(object_t *o);
extern bool deskelroom(room_t *room);

/* data.c */
extern bool initworldstate(worldstate_t *ws);
extern void destroyworldstate(worldstate_t *ws);

/* gamecore.c and {clievent,servevnt}.c */
extern void processevents(eventstack_t *evsk, void *obdat);
extern void updatephysics(worldstate_t *ws);
extern bool obtainobject(void *obdat_, objhandle_t handle, object_t **o);
extern bool obtainroom(void *obdat_, roomhandle_t handle, room_t **room);
extern void exitobject(void *obdat_, objhandle_t subject,
		       roomhandle_t location);

/* lua.c */
extern void setluaworldstate(worldstate_t *ws);
extern bool freezestate(lua_State *L, byte **data, dword *len);
extern bool meltstate(lua_State *L, char *data);

/* lua.c and {clilua,servlua}.c */
extern int setcuranimlua(lua_State *L);
extern int animhasloopedlua(lua_State *L);
extern int pushverblua(lua_State *L);
extern int tostringlua(lua_State *L);
extern int typelua(lua_State *L);
extern int setobjectlua(lua_State *L);
extern int getobjectlua(lua_State *L);
extern void setluaenv(lua_State *L);
extern int talklua(lua_State *L);

/* EOF fobwart.h */
