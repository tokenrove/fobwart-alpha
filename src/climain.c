/* 
 * climain.c ($Id$)
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
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
#include <dentata/random.h>
#include <dentata/util.h>

#include <lua.h>

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"


void mainloop(gamedata_t *gd);
void loginloop(gamedata_t *gd, char **uname, char **password);
void updatedecor(gamedata_t *gd, object_t *player);
void updatemanager(gamedata_t *gd, object_t *o);
void cameramanage(gamedata_t *gd, object_t *player);
void updatetext(gamedata_t *gd);
bool loaddata(gamedata_t *gd);
void destroydata(gamedata_t *gd);


int main(int argc, char **argv)
{
    gamedata_t gd;
    bool status;
    char *server = "localhost", *uname = "", *password = "";

    if(argc > 1)
        server = argv[1];

    /* initialize network */
    status = net_newclient(&gd.nh, server, FOBPORT);
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

    setluaworldstate(&gd.ws);

    /* load data from server */
    status = loaddata(&gd);
    if(status == failure) {
        d_error_fatal("Data load failed.\n");
        return 1;
    }

    loginloop(&gd, &uname, &password);

    /* login */
    status = net_login(gd.nh, uname, password, &gd.localobj);
    if(status != success) {
        d_error_fatal("Login as %s failed.\n", uname);
        return 1;
    }

    /* enter main loop */
    mainloop(&gd);

    /* close connection */
    /* deinit local */
    evsk_delete(&gd.evsk);
    deinitlocal(&gd);
    net_close(gd.nh);
    destroydata(&gd);
    destroyworldstate(&gd.ws);
    d_error_dump();
    return 0;
}


void loginloop(gamedata_t *gd, char **uname, char **password)
{
    d_point_t pt;
    d_rect_t r;
    int i, onfield;
    byte *logprompt = (byte *)"login: ", *passprompt = (byte *)"password: ";
    d_font_t *lshadow;
    char *field;
    char *motd[5] = { NULL, "fobwart alpha-zero",
                      "Copyright 2001 by Daniel Barnes and ",
                      "                  Julian Squires.",
                      NULL, };

    d_raster_setpalette(&gd->raster->palette);
    lshadow = d_font_dup(gd->largefont);
    d_font_silhouette(lshadow, d_color_fromrgb(gd->raster, 0, 0, 0), 255);
    onfield = 0;

    while(1) {
        d_event_update();
        i = handletextinput(&gd->type, gd->bounce);
        if(i == 1) {
            if(gd->type.pos > 0) {
                field = d_memory_new(gd->type.pos+1);
                d_memory_copy(field, gd->type.buf, gd->type.pos+1);
                d_memory_set(gd->type.buf, 0, gd->type.nalloc);
                gd->type.pos = 0;
                if(onfield == 0) {
                    *uname = field;
                    onfield++;
                } else if(onfield == 1) {
                    *password = field;
                    break;
                }
            }
        }

        decor_ll_mm2screen(gd->raster);

        r.offset.x = 0; r.w = gd->raster->desc.w;
        r.offset.y = 10; r.h = gd->largefont->desc.h+4;
        decor_ll_mm2window(gd->raster, r);

        pt.x = 6;
        pt.y = r.offset.y+4;
        d_font_printf(gd->raster, lshadow, pt, logprompt);
        pt.x--; pt.y--;
        d_font_printf(gd->raster, gd->largefont, pt, logprompt);
        pt.x += d_font_gettextwidth(gd->largefont, logprompt);
        pt.y += (gd->largefont->desc.h-gd->deffont->desc.h)/2;

        if(onfield == 0) {
            if(gd->type.buf)
                d_font_printf(gd->raster, gd->deffont, pt, (byte *)"%s",
                              gd->type.buf);
            pt.x += gd->type.pos*gd->deffont->desc.w;
            d_font_printf(gd->raster, gd->deffont, pt, (byte *)"\x10");
        } else
            d_font_printf(gd->raster, gd->deffont, pt, (byte *)"%s", *uname);

        r.offset.x = 0; r.w = gd->raster->desc.w;
        r.offset.y += r.h+10; r.h = gd->largefont->desc.h+4;
        decor_ll_mm2window(gd->raster, r);

        pt.x = 6;
        pt.y = r.offset.y+4;
        d_font_printf(gd->raster, lshadow, pt, passprompt);
        pt.x--; pt.y--;
        d_font_printf(gd->raster, gd->largefont, pt, passprompt);
        pt.x += d_font_gettextwidth(gd->largefont, passprompt);
        pt.y += (gd->largefont->desc.h-gd->deffont->desc.h)/2;

        if(onfield == 1) {
            if(gd->type.buf)
                d_font_printf(gd->raster, gd->deffont, pt, (byte *)"%s",
                              gd->type.buf);
            pt.x += gd->type.pos*gd->deffont->desc.w;
            d_font_printf(gd->raster, gd->deffont, pt, (byte *)"\x10");
        } else
            d_font_printf(gd->raster, gd->deffont, pt, (byte *)"%s",
                          *password);

        pt.x = 5;
        pt.y = gd->raster->desc.h-4*gd->largefont->desc.h;

        for(i = 1; motd[i] != NULL; i++) {
            pt.x++; pt.y++;
            d_font_printf(gd->raster, lshadow, pt, (byte *)"%s", motd[i]);
            pt.x--; pt.y--;
            d_font_printf(gd->raster, gd->largefont, pt, (byte *)"%s",
                          motd[i]);
            pt.y += gd->largefont->desc.h;
        }

        pt.x = pt.y = 0;
        d_raster_update();
    }

    d_font_delete(lshadow);
    return;
}


void mainloop(gamedata_t *gd)
{
    d_timehandle_t *th;
    d_point_t pt;
    object_t *o;
    char *greeting = "READY";

    getobject(gd, gd->localobj);
    if(d_set_fetch(gd->ws.objs, gd->localobj, (void **)&o) != success) {
        d_error_debug("ack. couldn't fetch localobj!\n");
        return;
    }
    gd->type.pos = 0;
    gd->type.nalloc = 0;
    gd->type.buf = NULL;
    gd->type.done = false;
    gd->quitcount = 0;
    gd->status = NULL;
    gd->readycount = 40;

    if(gd->hasaudio && gd->cursong != NULL)
        forkaudiothread(gd->cursong);

    while(1) {
        th = d_time_startcount(gd->fps, false);
        d_event_update();

        /* figure out what the player is pressing */
        /* emergency exit key */
        if(d_event_ispressed(EV_QUIT) && !gd->bounce[EV_QUIT])
            if(++gd->quitcount > 1)
                break;

        while(evsk_pop(&gd->evsk, NULL));
        handleinput(gd);

        net_syncevents(gd->nh, &gd->evsk);

        processevents(&gd->evsk, (void *)gd);
        updatephysics(&gd->ws);

        /* update manager/graphics */
        updatemanager(gd, o);
        /* update decor */
        updatedecor(gd, o);
        /* update text */
        updatetext(gd);

        pt.x = (gd->raster->desc.w-
                d_font_gettextwidth(gd->largefont,
                                    (byte *)"%s", greeting))/2;
        pt.y = (gd->raster->desc.h-gd->largefont->desc.h)/2-20;
        if(gd->readycount > 0)
            gd->readycount--;

        if(gd->readycount%10 > 5)
            d_font_printf(gd->raster, gd->largefont, pt, (byte *)"%s",
                          greeting);

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
    ebar_draw(gd->ebar, d_color_fromrgb(gd->ebar, 255, 255, 190),
              player->hp, player->maxhp);
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


void updatemanager(gamedata_t *gd, object_t *o)
{
    room_t *room;
    d_iterator_t it;
    dword key;
    object_t *o2;

    d_set_fetch(gd->ws.rooms, o->location, (void **)&room);
    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, gd->ws.objs), key != D_SET_INVALIDKEY) {
        d_set_fetch(gd->ws.objs, key, (void **)&o2);
        d_manager_jumpsprite(o2->sphandle, o2->x, o2->y);
    }
    cameramanage(gd, o);
    d_manager_draw(gd->raster);
    if(room->islit)
        gd->curpalette = &gd->light;
    else
        gd->curpalette = &gd->dark;
    d_raster_setpalette(gd->curpalette);
    return;
}


void cameramanage(gamedata_t *gd, object_t *player)
{
    int x, y;
    d_rect_t r;

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

    /* update text */
    pt.y = 202; pt.x = 2;
    if(gd->evmode == textinput) {
        if(gd->type.buf)
            d_font_printf(gd->raster, gd->deffont, pt, (byte *)
                          gd->type.buf);
        pt.x = 2+gd->type.pos*gd->deffont->desc.w;
        d_font_printf(gd->raster, gd->deffont, pt, (byte *)"\x10");
    }
    
    return;
}


bool loaddata(gamedata_t *gd)
{
    bool status;

    gd->deffont = d_font_load(DEFFONTFNAME);
    if(gd->deffont == NULL) return failure;
    gd->largefont = d_font_load(LARGEFONTFNAME);
    if(gd->largefont == NULL) return failure;
    d_font_convertdepth(gd->deffont, gd->raster->desc.bpp);
    d_font_convertdepth(gd->largefont, gd->raster->desc.bpp);

    /* load palette */
    loadpalette(LIGHTPALFNAME, &gd->light);
    loadpalette(DARKPALFNAME, &gd->dark);
    d_memory_copy(&gd->raster->palette, &gd->light,
                  D_NCLUTITEMS*D_BYTESPERCOLOR);

    d_font_silhouette(gd->largefont,
                      d_color_fromrgb(gd->raster, 255, 255, 255),
                      255);
    d_font_silhouette(gd->deffont, d_color_fromrgb(gd->raster, 0, 0, 15),
                      255);

    status = d_manager_new();
    if(status == failure)
        return failure;

    d_manager_setscrollparameters(true, 0);

    gd->ebar = ebar_new(gd->raster);

    if(gd->hasaudio) {
        gd->cursong = d_s3m_load(DATADIR "/mm2.s3m");
    }
    return success;
}


void destroydata(gamedata_t *gd)
{
    if(gd->hasaudio)
        d_s3m_delete(gd->cursong);

    d_image_delete(gd->ebar);
    d_manager_delete();

    d_font_delete(gd->largefont);
    gd->largefont = NULL;
    d_font_delete(gd->deffont);
    gd->deffont = NULL;
    return;
}

/* EOF climain.c */
