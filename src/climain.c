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


int gameloop(gamedata_t *gd);
bool clientinit(gamedata_t *gd, char *server, int port);
void updatedecor(gamedata_t *gd, object_t *player);
void updatemanager(gamedata_t *gd, object_t *o);
void cameramanage(gamedata_t *gd, object_t *player);
void updatetext(gamedata_t *gd);
void updategreeting(gamedata_t *gd, char *greeting);


int main(int argc, char **argv)
{
    gamedata_t gd;
    bool status;
    int mode;
    char *server = "localhost";

    if(argc > 1)
        server = argv[1];

    if(clientinit(&gd, server, FOBPORT) != success)
	return 1;

    /* login */
    status = loginloop(&gd);
    if(status != success)
        return 1;


    /* enter main loop */
    mode = 1;
    while(mode != -1) {
	switch(mode) {
	case 0:
	    mode = gameloop(&gd);
	    break;

	case 1:
	    mode = tmapmode(&gd);
	    break;

	default:
	    d_error_debug(__FUNCTION__": Unknown mode.\n");
	    mode = -1;
	    break;
	}
    }


    /* close connection */
    net_close(gd.nh);

    /* deinit local */
    evsk_delete(&gd.ws.evsk);
    deinitlocal(&gd);

    /* blow up the outside world */
    destroydata(&gd);
    destroyworldstate(&gd.ws);

    /* destroy msgbuf 
    msgbuf_destroy(&gd.msgbuf);
    */

    /* dump any extra errors. */
    d_error_dump();

    return 0;
}


bool clientinit(gamedata_t *gd, char *server, int port)
{
    bool status;

    /* initialize network */
    status = net_newclient(&gd->nh, server, port);
    if(status == failure) {
        d_error_fatal("Network init failed.\n");
        return failure;
    }

    /* initialize local */
    status = initlocal(gd);
    if(status == failure) {
        d_error_fatal("Local init failed.\n");
        return failure;
    }

    evsk_new(&gd->ws.evsk);

    /* setup a clean world state */
    status = initworldstate(&gd->ws);
    if(status != success) {
        d_error_fatal("Worldstate init failed.\n");
        return failure;
    }

    setluaworldstate(&gd->ws);

    /* initialize message buffer */
    msgbuf_init(&gd->msgbuf, MSGBUF_SIZE);
    setluamsgbuf(&gd->msgbuf);

    /* load data from server */
    status = loaddata(gd);
    if(status != success) {
        d_error_fatal("Data load failed.\n");
        return failure;
    }

    /* Initialize manager */
    status = d_manager_new();
    if(status == failure)
        return failure;

    d_manager_setscrollparameters(true, 0);

    return success;
}


/* gameloop
 * Performs the main input-update-output loop most commonly thought of
 * as the game. */
int gameloop(gamedata_t *gd)
{
    d_timehandle_t *th;
    object_t *o;

    getobject(gd, gd->localobj);
    if(d_set_fetch(gd->ws.objs, gd->localobj, (void **)&o) != success) {
        d_error_debug("ack. couldn't fetch localobj!\n");
        return -1;
    }
    getroom(gd, o->location);

    /* Reset type buffer */
    gd->type.pos = 0;
    gd->type.nalloc = 0;
    gd->type.buf = NULL;
    gd->type.done = false;

    /* Reset some miscellaneous variables */
    gd->quitcount = 0;
    gd->status = NULL;
    gd->readycount = 40;

    /* music is disabled atm.
    if(gd->hasaudio && gd->cursong != NULL)
        forkaudiothread(gd->cursong);
    */


    /* gd->focuswidget = 0; */

    while(1) {
        th = d_time_startcount(gd->fps, false);
        d_event_update();

        /* emergency exit key */
        if(d_event_ispressed(EV_QUIT) && !gd->bounce[EV_QUIT]) {
	    break;
	}

	/*
	  gd->widgets[gd->focuswidget].input(gd);
	*/
        handleinput(gd);

        net_syncevents(gd->nh, &gd->ws.evsk);

        processevents(&gd->ws.evsk, (void *)gd);
        updatephysics(&gd->ws);

        while(evsk_pop(&gd->ws.evsk, NULL));

	/*
	  for(i = 0; i < gs->nwidgets; i++)
	      gd->widgets[i].update(gd);
	*/
        /* update manager/graphics */
        if(d_set_fetch(gd->ws.objs, gd->localobj, (void **)&o) != success) {
            d_error_debug("ack. couldn't fetch localobj!\n");
            return -1;
        }
        updatemanager(gd, o);
        /* update decor */
        updatedecor(gd, o);
        /* update text */
        updatetext(gd);
	/* update greeting message */
	updategreeting(gd, "READY");

        d_raster_update();
        d_time_endcount(th);
    }

    return -1;
}


/* updategreeting
 * display a flashing message in the center of the screen, while
 * the ``readycount'' is still valid. (it's reset on a new game and
 * on a death) */
void updategreeting(gamedata_t *gd, char *greeting)
{
    d_point_t pt;

    if(gd->readycount > 0) {
	pt.x = (gd->raster->desc.w-
		d_font_gettextwidth(gd->largefont,
				    (byte *)"%s", greeting))/2;
	pt.y = (gd->raster->desc.h-gd->largefont->desc.h)/2-20;
	gd->readycount--;
	
	if(gd->readycount%8 > 4)
	    d_font_printf(gd->raster, gd->largefont, pt, (byte *)"%s",
			  greeting);
    }
    return;
}


void updatedecor(gamedata_t *gd, object_t *player)
{
    d_point_t pt;
    d_color_t c;

    /* Grab the message box color. */
    d_memory_copy(&gd->ebar->palette, gd->curpalette,
		  D_NCLUTITEMS*D_BYTESPERCOLOR);
    c = d_color_fromrgb(gd->ebar, 220, 220, 170);

    /* Draw player's energy bar. */
    ebar_draw(gd->ebar, d_color_fromrgb(gd->ebar, 255, 255, 190),
              player->hp, player->maxhp);
    pt.x = 8;
    pt.y = 8;
    d_image_blit(gd->raster, gd->ebar, pt);

    /* Draw message box. */
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
    /* Note: height of text window */
    if(y+(gd->raster->desc.h-40) > r.h) y = r.h-(gd->raster->desc.h-40);
    if(x < 0) x = 0;
    if(y < 0) y = 0;
    d_manager_jump(x, y);
    return;
}


void updatetext(gamedata_t *gd)
{
    d_point_t pt;
    msgbufline_t *p;

    /* update text */
    pt.y = 202; pt.x = 2;
    if(gd->evmode == textinput) {
	if(gd->type.buf) {
	    gd->type.buf[gd->type.pos] = 0;
            d_font_printf(gd->raster, gd->deffont, pt, (byte *)
                          gd->type.buf);
	}
        pt.x = 2+gd->type.pos*gd->deffont->desc.w;
        d_font_printf(gd->raster, gd->deffont, pt, (byte *)"\x10");
    }

    pt.x = 2; pt.y += gd->deffont->desc.h+1;
    for(p = gd->msgbuf.current; p->line.data && p != gd->msgbuf.bottom &&
	    pt.y < gd->raster->desc.h-gd->deffont->desc.h+1; p = p->next) {
	d_font_printf(gd->raster, gd->deffont, pt, (byte *)"%s", p->line.data);

	pt.y += gd->deffont->desc.h+1;
    }

    return;
}


/* EOF climain.c */
