/* 
 * local.c
 * Created: Sat Jul 14 23:30:18 2001 by tek@wiw.org
 * Revised: Wed Jul 25 07:11:11 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

#include <dentata/types.h>
#include <dentata/image.h>
#include <dentata/raster.h>
#include <dentata/event.h>
#include <dentata/font.h>
#include <dentata/sprite.h>
#include <dentata/tilemap.h>
#include <dentata/sample.h>
#include <dentata/audio.h>
#include <dentata/s3m.h>
#include <dentata/memory.h>
#include <dentata/error.h>
#include <dentata/set.h>

#include <lua.h>

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"


bool initlocal(gamedata_t *gd);
void deinitlocal(gamedata_t *gd);
void audioinit(gamedata_t *gd);
void eventinit(gamedata_t *gd);
bool isbounce(byte ev);
void debouncecontrols(void);

static bool bounce[EV_LAST];


bool initlocal(gamedata_t *gd)
{
    bool status;
    d_rasterdescription_t mode;

    status = d_raster_new();
    if(status == failure)
        return failure;

    /* FIXME insert mode haggle here */
    mode.w = IDEALWIDTH;
    mode.h = IDEALHEIGHT;
    mode.bpp = IDEALBPP;
    mode.cspace = RGB;
    mode.alpha = 0;
    if(mode.bpp == 8) mode.paletted = true;
    else mode.paletted = false;

    status = d_raster_setmode(mode);
    if(status == failure)
        return failure;

    gd->raster = d_image_new(mode);
    d_image_setdataptr(gd->raster, d_raster_getgfxptr(), false);

    eventinit(gd);

    gd->fps = TARGETFPS;
    gd->slowmo = false;

    audioinit(gd);
    return success;
}


void eventinit(gamedata_t *gd)
{
    int i;
    int keymap[48] = { D_KBD_GRAVE, D_KBD_1, D_KBD_2, D_KBD_3, D_KBD_4,
                       D_KBD_5, D_KBD_6, D_KBD_7, D_KBD_8, D_KBD_9,
                       D_KBD_0, D_KBD_MINUS, D_KBD_EQUALS, D_KBD_Q,
                       D_KBD_W, D_KBD_E, D_KBD_R, D_KBD_T, D_KBD_Y,
                       D_KBD_U, D_KBD_I, D_KBD_O, D_KBD_P, D_KBD_LBRACK,
                       D_KBD_RBRACK, D_KBD_BACKSLASH, D_KBD_A, D_KBD_S,
                       D_KBD_D, D_KBD_F, D_KBD_G, D_KBD_H, D_KBD_J,
                       D_KBD_K, D_KBD_L, D_KBD_SEMICOLON, D_KBD_APOSTROPHE,
                       D_KBD_Z, D_KBD_X, D_KBD_C, D_KBD_V, D_KBD_B,
                       D_KBD_N, D_KBD_M, D_KBD_COMMA, D_KBD_PERIOD,
                       D_KBD_SLASH, D_KBD_SPACE };

    d_event_new(D_EVENT_KEYBOARD);
    d_event_map(EV_QUIT, D_KBD_ESCAPE);
    d_event_map(EV_LEFT, D_KBD_CURSORLEFT);
    d_event_map(EV_RIGHT, D_KBD_CURSORRIGHT);
    d_event_map(EV_UP, D_KBD_CURSORUP);
    d_event_map(EV_DOWN, D_KBD_CURSORDOWN);
    d_event_map(EV_ACT, D_KBD_LEFTCONTROL);
    d_event_map(EV_ACT, D_KBD_Z);
    d_event_map(EV_JUMP, D_KBD_LEFTALT);
    d_event_map(EV_JUMP, D_KBD_X);
    d_event_map(EV_BACKSPACE, D_KBD_BS);
    d_event_map(EV_ENTER, D_KBD_ENTER);
    d_event_map(EV_SHIFT, D_KBD_LEFTSHIFT);
    d_event_map(EV_SHIFT, D_KBD_RIGHTSHIFT);
    d_event_map(EV_PAGEUP, D_KBD_PAGEUP);
    d_event_map(EV_PAGEDOWN, D_KBD_PAGEDOWN);
    d_event_map(EV_TAB, D_KBD_TAB);
    d_event_map(EV_CONSOLE, D_KBD_GRAVE);
    d_event_map(EV_INVENTORY, D_KBD_CAPSLOCK);
    d_event_map(EV_HELP, D_KBD_F1);
    d_event_map(EV_GAMEMODE, D_KBD_F2);
    d_event_map(EV_ROOMMODE, D_KBD_F3);

    for(i = EV_ALPHABEGIN; i < EV_ALPHAEND; i++)
        d_event_map(i, keymap[i-EV_ALPHABEGIN]);

    debouncecontrols();
    return;
}


void audioinit(gamedata_t *gd)
{
    d_audiomode_t mode;

    mode.frequency = 22050;
    mode.bitspersample = 8;
    mode.nchannels = 1;
    mode.encoding = PCM;
    gd->hasaudio = d_audio_new(mode);
    return;
}


void deinitlocal(gamedata_t *gd)
{
    if(gd->hasaudio)
        d_audio_delete();

    d_image_delete(gd->raster);
    d_event_delete();
    d_raster_delete();
    return;
}


void debouncecontrols(void)
{
    int i;

    for(i = 0; i < EV_LAST; i++)
        bounce[i] = d_event_ispressed(i);
}


bool isbounce(byte ev)
{
    return bounce[ev];
}

/* EOF local.c */
