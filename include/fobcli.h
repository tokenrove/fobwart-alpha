/* 
 * fobclient.h
 * Created: Thu Jul 19 19:18:44 2001 by tek@wiw.org
 * Revised: Fri Jul 27 00:40:08 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

#include <dentata/font.h>

/* Various constants */
enum {
    IDEALWIDTH = 320, IDEALHEIGHT = 240, IDEALBPP = 8, TARGETFPS = 30,
    MSGBUF_SIZE = 100,

    EV_QUIT = 0, EV_LEFT = 1, EV_RIGHT = 2, EV_UP = 3, EV_DOWN = 4,
    EV_ACT = 5, EV_JUMP = 6, EV_TALK = 7,
    EV_ALPHABEGIN = 8, EV_ALPHAEND = 56,  EV_BACKSPACE = 57,
    EV_ENTER = 58, EV_SHIFT = 59, EV_PAGEUP = 60, EV_PAGEDOWN = 61,
    EV_LAST = 62
};

#define LOGINPROMPT ((byte *)"login: ")
#define PASSPROMPT  ((byte *)"password: ")


typedef struct msgbufline_s {
    string_t line;
    struct msgbufline_s *next, *prev;
} msgbufline_t;

typedef struct msgbuf_s {
    msgbufline_t *head, *current, *bottom;
} msgbuf_t;

struct gamedata_s;

typedef struct widget_s {
    bool (*input)(struct gamedata_s *);
    void (*update)(struct gamedata_s *);
} widget_t;

typedef struct gamedata_s {
    d_image_t *raster;
    int fps;
    d_font_t *deffont, *largefont;
    bool hasaudio;

    worldstate_t ws;

    /* network/game related */
    nethandle_t *nh;

    /* event related */
    bool bounce[EV_LAST];
    typebuf_t type;
    byte *status;
    bool slowmo;
    int slowcount;
    enum {gameinput = 0, textinput = 1} evmode;
    int quitcount;
    int readycount;

    msgbuf_t msgbuf;

    int nwidgets;
    widget_t *widgets;

    /* room data */
    d_palette_t light, dark, *curpalette;

    /* object data */
    word localobj;

    /* decor data */
    d_image_t *ebar;

    /* audio data */
    d_s3m_t *cursong;
} gamedata_t;


/* local.c */
extern bool initlocal(gamedata_t *gd);
extern void deinitlocal(gamedata_t *gd);
extern void handleinput(gamedata_t *gd);

/* clidata.c */
extern bool loaddata(gamedata_t *gd);
extern void destroydata(gamedata_t *gd);

/* clinet.c */
extern bool getobject(gamedata_t *gd, word handle);
extern bool getroom(gamedata_t *gd, word handle);

/* clilua.c */
extern void setluamsgbuf(msgbuf_t *mb);

/* msgbuf.c */
extern void msgbuf_init(msgbuf_t *mb, int size);
extern void msgbuf_destroy(msgbuf_t *mb);

/* clilogin.c */
extern bool loginloop(gamedata_t *gd);
extern bool loginscreen(gamedata_t *gd, char **uname, char **password);

/* EOF fobclient.h */
