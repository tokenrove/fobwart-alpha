/* 
 * fobwart.h
 * Created: Sat Jul 14 23:23:21 2001 by tek@wiw.org
 * Revised: Thu Jul 19 19:22:21 2001 by tek@wiw.org
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

/* Event verbs */
enum {
    VERB_NOP = 0, VERB_TALK, VERB_RIGHT, VERB_LEFT, VERB_UP, VERB_DOWN,
    VERB_ACT, VERB_JUMP, VERB_DIE,
    VERB_PRE = 254, VERB_AUTO = 255
};

typedef word objhandle_t;
typedef word roomhandle_t;


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
    objhandle_t handle;

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
    roomhandle_t handle;

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
    word subject;
    byte verb;
    void *auxdata;
    dword auxlen;
} event_t;


typedef struct eventstack_s {
    int top, nalloc;
    event_t *events;
} eventstack_t;


/* Network related */
enum { PACK_YEAWHAW = 0, PACK_IRECKON, PACK_AYUP,
       PACK_LOGIN, PACK_SYNC, PACK_EVENT, PACK_FRAME,
       PACK_GETOBJECT, PACK_GETROOM };

typedef struct packet_s {
    byte type;
    union {
        byte handshake;
        word handle;
        struct {
            byte *name, *password;
            int namelen, pwlen;
        } login;
        event_t event;
    } body;
} packet_t;

extern bool readpack(int socket, packet_t *p);
extern bool writepack(int socket, packet_t p);

extern void evsk_new(eventstack_t *evsk);
extern void evsk_delete(eventstack_t *evsk);
extern void evsk_push(eventstack_t *evsk, event_t ev);
extern bool evsk_top(eventstack_t *evsk, event_t *ev);
extern bool evsk_pop(eventstack_t *evsk);
extern void messagebuf_add(messagebuf_t *m, byte *msg, dword msglen);
extern d_sprite_t *loadsprite(char *fname);
extern d_tilemap_t *loadtmap(char *filename);
extern void loadpalette(char *filename, d_palette_t *palette);

/* EOF fobwart.h */
