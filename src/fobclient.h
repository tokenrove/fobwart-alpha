/* 
 * fobclient.h
 * Created: Thu Jul 19 19:18:44 2001 by tek@wiw.org
 * Revised: Thu Jul 19 22:21:52 2001 by tek@wiw.org
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

    EBAR_MAXSLIVERS = 24
};

typedef struct gamedata_s {
    d_image_t *raster;
    int fps;
    d_font_t *deffont, *largefont;
    bool hasaudio;

    worldstate_t ws;

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
    d_palette_t light, dark, *curpalette;

    /* object data */
    word localobj;

    /* decor data */
    d_image_t *ebar;

    /* audio data */
    d_s3m_t *cursong;
} gamedata_t;


extern bool initlocal(gamedata_t *gd);
extern bool loaddata(gamedata_t *gd);
extern void destroydata(gamedata_t *gd);
extern void deinitlocal(gamedata_t *gd);
extern void handleinput(gamedata_t *gd);
extern void forkaudiothread(gamedata_t *);
extern void ebar_draw(d_image_t *bar, int nslivers);
extern void processevents(gamedata_t *gd);
extern bool initnet(gamedata_t *gd, char *servname, int port);
extern void closenet(gamedata_t *gd);
extern bool login(gamedata_t *gd, char *uname, char *password);
extern void syncevents(gamedata_t *gd);
extern bool getobject(gamedata_t *gd, word handle);
extern bool getroom(gamedata_t *gd, word handle);


/* EOF fobclient.h */
