/* 
 * main.c
 * Created: Sat Jul 14 23:07:02 2001 by tek@wiw.org
 * Revised: Thu Jul 19 22:02:38 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

#include <dentata/types.h>
#include <dentata/image.h>
#include <dentata/error.h>
#include <dentata/time.h>
#include <dentata/raster.h>
#include <dentata/event.h>
#include <dentata/font.h>
#include <dentata/sprite.h>
#include <dentata/tilemap.h>
#include <dentata/manager.h>
#include <dentata/sample.h>
#include <dentata/audio.h>
#include <dentata/s3m.h>
#include <dentata/set.h>
#include <dentata/color.h>
#include <dentata/memory.h>

#include <lua.h>

#include "fobwart.h"
#include "fobclient.h"

void mainloop(gamedata_t *gd);
void updatedecor(gamedata_t *gd, object_t *player);
void cameramanage(gamedata_t *gd, object_t *player);
void updatetext(gamedata_t *gd);

int main(int argc, char **argv)
{
    gamedata_t gd;
    bool status;
    char *server = "localhost";

    if(argc > 1)
        server = argv[1];

    /* initialize network */
    status = initnet(&gd, server, 6400);
    if(status == failure) {
        d_error_fatal("Network init failed.\n");
        return 1;
    }

    /* initialize local */
    status = initlocal(&gd);
    if(status == failure) {
        d_error_fatal("Local init failed.\n");
        return 1;
    }

    evsk_new(&gd.evsk);

    status = initworldstate(&gd.ws);
    if(status != success) {
        d_error_fatal("Worldstate init failed.\n");
        return 1;
    }

    gd.mbuf.nlines = gd.mbuf.curline = 0;
    gd.mbuf.maxlines = 42;
    gd.mbuf.lines = d_memory_new(sizeof(byte *)*gd.mbuf.maxlines);
    
    /* load data from server */
    status = loaddata(&gd);
    if(status == failure) {
        d_error_fatal("Data load failed.\n");
        return 1;
    }

    /* login */
    status = login(&gd, "tek", "dwarfinvasion");
    if(status != success) {
        d_error_fatal("Login as %s failed.\n", "tek");
        return 1;
    }

    /* enter main loop */
    mainloop(&gd);

    /* close connection */
    /* deinit local */
    evsk_delete(&gd.evsk);
    deinitlocal(&gd);
    closenet(&gd);
    destroydata(&gd);
    destroyworldstate(&gd.ws);
    d_error_dump();
    return 0;
}

void mainloop(gamedata_t *gd)
{
    d_timehandle_t *th;
    d_point_t pt;
    object_t *o;
    room_t *room;

    d_set_fetch(gd->ws.objs, gd->localobj, (void **)&o);
    d_set_fetch(gd->ws.rooms, o->location, (void **)&room);
    gd->type.pos = 0;
    gd->type.nalloc = 0;
    gd->type.buf = NULL;
    gd->type.done = false;
    gd->quitcount = 0;
    gd->status = NULL;
    gd->readycount = 40;

    if(gd->hasaudio && gd->cursong != NULL)
        forkaudiothread(gd);

    while(1) {
        th = d_time_startcount(gd->fps, false);
        d_event_update();

        /* figure out what the player is pressing */
        /* emergency exit key */
        if(d_event_ispressed(EV_QUIT) && !gd->bounce[EV_QUIT])
            if(++gd->quitcount > 1)
                break;

        while(evsk_pop(&gd->evsk));
        handleinput(gd);
        /* dispatch events */
        /* sync info with server */
        syncevents(gd);
        /* do physics calculation */
        processevents(gd);
        updatephysics(&gd->ws);

        /* update manager/graphics */
        d_set_fetch(gd->ws.rooms, o->location, (void **)&room);
        cameramanage(gd, o);
        d_manager_draw(gd->raster);
        if(room->islit)
            gd->curpalette = &gd->light;
        else
            gd->curpalette = &gd->dark;
        d_raster_setpalette(gd->curpalette);

        /* update decor */
        updatedecor(gd, o);
        /* update text */
        updatetext(gd);

        pt.x = 140;
        pt.y = 90;
        if(gd->readycount > 0)
            gd->readycount--;

        if(gd->readycount%10 > 5)
            d_font_printf(gd->raster, gd->largefont, pt, (byte *)"GOOD MORNING");

        d_raster_update();
        d_time_endcount(th);
    }

    return;
}

void updatedecor(gamedata_t *gd, object_t *player)
{
    d_point_t pt;
    d_color_t c;

    d_memory_copy(&gd->ebar->palette, gd->curpalette,
                  D_NCLUTITEMS*D_BYTESPERCOLOR);
    c = d_color_fromrgb(gd->ebar, 220, 220, 170);
    ebar_draw(gd->ebar, player->hp*EBAR_MAXSLIVERS/player->maxhp);
    pt.x = 8;
    pt.y = 8;
    d_image_blit(gd->raster, gd->ebar, pt);

    pt.x = 0;
    pt.y = 200;
    for(pt.y = 200; pt.y < gd->raster->desc.h; pt.y++)
        for(pt.x = 0; pt.x < gd->raster->desc.w; pt.x++) {
            d_image_setpelcolor(gd->raster, pt, c);
        }

    return;
}

void cameramanage(gamedata_t *gd, object_t *player)
{
    int x, y;
    d_rect_t r;

    d_manager_jumpsprite(player->sphandle, player->x, player->y);
    r = d_manager_getvirtualrect();
    x = player->x-gd->raster->desc.w/2;
    y = player->y-gd->raster->desc.h/2;
    if(x+gd->raster->desc.w > r.w) x = r.w-gd->raster->desc.w;
    if(y+gd->raster->desc.h > r.h) y = r.h-gd->raster->desc.h;
    if(x < 0) x = 0;
    if(y < 0) y = 0;
    d_manager_jump(x, y);
    return;
}

void updatetext(gamedata_t *gd)
{
    d_point_t pt;
    int i;

    /* update text */
    pt.y = 202; pt.x = 2;
    if(gd->evmode == textinput) {
        if(gd->type.buf)
            d_font_printf(gd->raster, gd->deffont, pt, (byte *)
                          gd->type.buf);
        pt.x = 2+gd->type.pos*gd->deffont->desc.w;
        d_font_printf(gd->raster, gd->deffont, pt, (byte *)"\x10");
    }
    
    pt.x = 2;
    pt.y += gd->deffont->desc.h;
    for(i = gd->mbuf.curline; i < gd->mbuf.nlines &&
            pt.y+8 < gd->raster->desc.h; i++, pt.y+=gd->deffont->desc.h) {
        d_font_printf(gd->raster, gd->deffont, pt, gd->mbuf.lines[i]);
    }
    return;
}

/* EOF main.c */
