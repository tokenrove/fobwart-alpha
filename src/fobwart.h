/* 
 * fobwart.h
 * Created: Sat Jul 14 23:23:21 2001 by tek@wiw.org
 * Revised: Wed Jul 18 02:24:02 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

/* Various constants */
enum {
    IDEALWIDTH = 320, IDEALHEIGHT = 240, IDEALBPP = 8, TARGETFPS = 60,

    EV_QUIT = 0, EV_LEFT = 1, EV_RIGHT = 2, EV_UP = 3, EV_DOWN = 4,
    EV_ACT = 5, EV_JUMP = 6, EV_TALK = 7,
    EV_ALPHABEGIN = 8, EV_ALPHAEND = 56,  EV_BACKSPACE = 57,
    EV_ENTER = 58, EV_SHIFT = 59, EV_PAGEUP = 60, EV_PAGEDOWN = 61,
    EV_LAST = 62,

    XVELOCITYMAX = 4, XACCELMAX = 2, JUMPVELOCITY = 7,

    ANIMSTANDLEFT = 0, ANIMSTANDRIGHT = 1,
    ANIMRUNLEFT = 2, ANIMRUNRIGHT = 3,
    ANIMJUMPLEFT = 4, ANIMJUMPRIGHT = 5,

    EBAR_MAXSLIVERS = 24
};

#define DATADIR "./data"
#define DEFFONTFNAME   DATADIR "/def.fnt"
#define LARGEFONTFNAME DATADIR "/slant.fnt"
#define LIGHTPALFNAME  DATADIR "/light.pal"
#define DARKPALFNAME   DATADIR "/dark.pal"

/* Event verbs */
enum {
    VERB_NOP = 0, VERB_TALK, VERB_RIGHT, VERB_LEFT, VERB_UP, VERB_DOWN,
    VERB_ACT, VERB_JUMP, VERB_DIE,
    VERB_PRE = 254, VERB_AUTO = 255
};


typedef struct typebuf_s {
    byte *buf;
    word pos, nalloc;
    bool done;
} typebuf_t;


typedef struct messagebuf_s {
    int curline, nlines, maxlines;
    byte **lines;
} messagebuf_t;


typedef struct object_s {
    char *name;
    word handle;

    /* physics */
    word x, y;
    int ax, ay;
    int vx, vy;
    bool onground;
    enum { left = 0, right = 1 } facing;

    /* statistics */
    word hp, maxhp;

    /* graphics */
    d_sprite_t *sprite;
    word sphandle;

    /* scripts */
    d_set_t *scripts;
} object_t;


typedef struct room_s {
    char *name;
    word handle;

    /* physics */
    int gravity;
    bool islit;

    /* tilemaps */
    d_tilemap_t *map;
    word tmhandle;

    /* contents */
    d_set_t *objs;

    /* scripts */
} room_t;


typedef struct event_s {
    word subject, object;
    byte verb;
    void *auxdata;
    dword auxlen;
} event_t;


typedef struct eventstack_s {
    int top, nalloc;
    event_t *events;
} eventstack_t;


typedef struct gamedata_s {
    d_image_t *raster;
    int fps;
    d_font_t *deffont, *largefont;
    bool hasaudio;
    lua_State *luastate;

    /* network/game related */
    int socket;
    messagebuf_t mbuf;

    /* event related */
    bool bounce[EV_LAST];
    typebuf_t type;
    byte *status;
    bool slowmo;
    int slowcount;
    enum {gameinput = 0, textinput = 1} evmode;
    int quitcount;
    int readycount;
    eventstack_t evsk;

    /* room data */
    d_set_t *rooms;
    word curroom;
    d_palette_t light, dark, *curpalette;

    /* object data */
    d_set_t *objs;
    word localobj;

    /* decor data */
    d_image_t *ebar;

    /* audio data */
    d_s3m_t *cursong;
} gamedata_t;


extern void evsk_new(eventstack_t *evsk);
extern void evsk_delete(eventstack_t *evsk);
extern void evsk_push(eventstack_t *evsk, event_t ev);
extern bool evsk_top(eventstack_t *evsk, event_t *ev);
extern bool evsk_pop(eventstack_t *evsk);
extern bool initlocal(gamedata_t *gd);
extern bool loaddata(gamedata_t *gd);
extern void deinitlocal(gamedata_t *gd);
extern void handleinput(gamedata_t *gd);
extern void updatephysics(gamedata_t *gd);
extern void forkaudiothread(gamedata_t *);
extern void ebar_draw(d_image_t *bar, int nslivers);
extern void processevents(gamedata_t *gd);
extern void messagebuf_add(messagebuf_t *m, byte *msg, dword msglen);
extern bool initnet(gamedata_t *gd, char *servname, int port);
extern void closenet(gamedata_t *gd);
extern bool login(gamedata_t *gd, char *uname, char *password);
extern void syncevents(gamedata_t *gd);

/* EOF fobwart.h */
