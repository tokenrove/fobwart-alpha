/* 
 * fobwart.h
 * Created: Sat Jul 14 23:23:21 2001 by tek@wiw.org
 * Revised: Fri Jul 27 00:23:42 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

#define DATADIR "./data"
#define DEFFONTFNAME   DATADIR "/def.fnt"
#define LARGEFONTFNAME DATADIR "/slant.fnt"
#define LIGHTPALFNAME  DATADIR "/light.pal"
#define DARKPALFNAME   DATADIR "/dark.pal"
#define DEFOBJSCRIPTFNAME DATADIR "/defobj.luc"

enum { FOBPORT = 6400 };

/* Event verbs */
enum {
    VERB_NOP = 0, VERB_TALK, VERB_RIGHT, VERB_LEFT, VERB_UP, VERB_DOWN,
    VERB_ACT, VERB_AUTO
};

/* Other game constants */
enum { OBJSTACKSIZE = 0, LUAOBJECT_TAG = 0 };

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
    int ax, ay;
    int vx, vy;
    bool onground;

    /* statistics */
    word hp, maxhp;

    /* graphics */
    char *spname;
    d_sprite_t *sprite;
    word sphandle;

    /* scripts */
    lua_State *luastate;
    char *statebuf;
} object_t;


typedef struct room_s {
    char *name;
    roomhandle_t handle;

    /* physics */
    int gravity;
    bool islit;

    /* graphics */
    char *mapname;
    d_tilemap_t *map;
    word tmhandle;

    char *bgname;
    d_image_t *bg;
    word bghandle;

    /* contents */
    d_set_t *contents;

    /* scripts */
} room_t;


typedef struct event_s {
    word subject;
    byte verb;
    void *auxdata;
    dword auxlen;
} event_t;


typedef struct worldstate_s {
    d_set_t *objs;
    d_set_t *rooms;
    lua_State *luastate;
} worldstate_t;


typedef struct eventstack_s {
    int top, nalloc;
    event_t *events;
} eventstack_t;


extern void evsk_new(eventstack_t *evsk);
extern void evsk_delete(eventstack_t *evsk);
extern void evsk_push(eventstack_t *evsk, event_t ev);
extern bool evsk_top(eventstack_t *evsk, event_t *ev);
extern bool evsk_pop(eventstack_t *evsk, event_t *ev);

extern bool string_fromasciiz(string_t *dst, const char *src);
extern void string_delete(string_t *s);

extern d_sprite_t *loadsprite(char *fname);
extern d_tilemap_t *loadtmap(char *filename);
extern bool loadpalette(char *filename, d_palette_t *palette);
extern bool loadscript(lua_State *L, char *filename);
extern bool deskelobject(object_t *o);
extern bool deskelroom(room_t *room);

extern bool initworldstate(worldstate_t *ws);
extern void destroyworldstate(worldstate_t *ws);

extern void processevents(eventstack_t *evsk, void *obdat);
extern void updatephysics(worldstate_t *ws);
extern bool obtainobject(void *obdat_, objhandle_t handle, object_t **o);
extern bool obtainroom(void *obdat_, roomhandle_t handle, room_t **room);

extern void setluaworldstate(worldstate_t *ws);
extern bool freezestate(lua_State *L, byte **data, dword *len);
extern bool meltstate(lua_State *L, char *data);

extern int setcuranimlua(lua_State *L);
extern int tostringlua(lua_State *L);
extern int typelua(lua_State *L);
extern int setobjectlua(lua_State *L);
extern int getobjectlua(lua_State *L);

extern void forkaudiothread(d_s3m_t *song);

extern d_image_t *ebar_new(d_image_t *raster);
extern void ebar_draw(d_image_t *bar, d_color_t primary, int a, int b);

extern void decor_ll_mm2screen(d_image_t *bg);
extern void decor_ll_mm2window(d_image_t *bg, d_rect_t r);

extern void debouncecontrols(bool *bounce);
extern void insertchar(typebuf_t *type, int i, bool shift);
extern int handletextinput(typebuf_t *type, bool *bounce);

extern void setluaenv(lua_State *L);
extern int talklua(lua_State *L);


/* EOF fobwart.h */
