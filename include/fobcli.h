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
#include <dentata/sample.h>
#include <dentata/s3m.h>

/* Various constants */
enum {
    IDEALWIDTH = 320, IDEALHEIGHT = 240, IDEALBPP = 8, TARGETFPS = 30,
    MSGBUF_SIZE = 100,

    EV_QUIT = 0, EV_LEFT = 1, EV_RIGHT = 2, EV_UP = 3, EV_DOWN = 4,
    EV_ACT = 5, EV_JUMP = 6, EV_CONSOLE = 7,
    EV_ALPHABEGIN = 8, EV_ALPHAEND = 56,  EV_BACKSPACE = 57,
    EV_ENTER = 58, EV_SHIFT = 59, EV_PAGEUP = 60, EV_PAGEDOWN = 61,
    EV_TAB = 62, EV_INVENTORY = 63, EV_HELP = 64, EV_GAMEMODE = 65,
    EV_ROOMMODE = 66, EV_LAST = 67,

    WIDGET_GAME = 0, WIDGET_MSGBOX, WIDGET_EBAR, WIDGET_GREETING,
    WIDGET_QUIT, WIDGET_INVENTORY, WIDGET_CONSOLE, WIDGET_MAPEDIT
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
    bool takesfocus, hasfocus;
    int type;
    void *private;

    void (*input)(struct widget_s *);
    bool (*update)(struct widget_s *);
    void (*close)(struct widget_s *);
} widget_t;

typedef struct widgetstack_s {
	int top, nalloced, focus;
	widget_t **stack;
} widgetstack_t;

typedef struct gamedata_s {
    d_image_t *raster;
    int fps;
    d_font_t *deffont, *largefont;
    bool hasaudio;

    worldstate_t ws;

    /* network/game related */
    nethandle_t *nh;

    /* event related */
    byte *status;
    bool slowmo;
    int slowcount;
    enum {gameinput = 0, textinput = 1} evmode;
    int quitcount;

    /* room data */
    d_palette_t light, dark, *curpalette;

    /* object data */
    word localobj;

    /* audio data */
    d_s3m_t *cursong;
} gamedata_t;


/* local.c */
extern bool initlocal(gamedata_t *gd);
extern void deinitlocal(gamedata_t *gd);
extern void handleinput(gamedata_t *gd);
extern bool isbounce(byte ev);
extern void debouncecontrols(void);

/* clidata.c */
extern bool loaddata(gamedata_t *gd);
extern void destroydata(gamedata_t *gd);

/* clinet.c */
extern bool net_sync(gamedata_t *gd);
extern bool handlepacket(gamedata_t *gd);
extern bool expectpacket(gamedata_t *gd, int waittype, packet_t *p);
extern bool getobject(gamedata_t *gd, word handle);
extern bool getroom(gamedata_t *gd, word handle);

/* clilua.c */
extern void setluamsgbuf(msgbuf_t *mb);

/* msgbuf.c */
extern void msgbuf_init(msgbuf_t *mb, int size);
extern void msgbuf_destroy(msgbuf_t *mb);
extern widget_t *msgboxwidget_init(gamedata_t *gd);

/* dialog.c */
extern widget_t *yesnodialog_init(gamedata_t *gd, char *msg, int type);
extern bool yesnodialog_return(widget_t *wp);

/* debugcon.c */
extern widget_t *debugconsole_init(gamedata_t *gd);

/* clilogin.c */
extern bool loginloop(gamedata_t *gd);
extern bool loginscreen(gamedata_t *gd, char **uname, char **password);

/* gamewidg.c */
extern widget_t *gamewidget_init(gamedata_t *gd);

/* ebarwidg.c */
extern widget_t *ebarwidget_init(gamedata_t *gd);

/* greetwdg.c */
extern widget_t *greetingwidget_init(gamedata_t *gd, char *msg);

/* type.c */
extern void insertchar(typebuf_t *type, int i, bool shift);
extern int handletextinput(typebuf_t *type);

/* unxaudio.c */
extern void forkaudiothread(d_s3m_t *song);

/* decor.c */
extern void decor_ll_mm2screen(d_image_t *bg);
extern void decor_ll_mm2window(d_image_t *bg, d_rect_t r);

/* rmedmode.c */
extern int roomeditloop(gamedata_t *gd);

/* mapeditw.c */
extern widget_t *mapeditwidget_init(gamedata_t *gd);

/* EOF fobclient.h */
