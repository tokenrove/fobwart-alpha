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
bool loginloop(gamedata_t *gd);
bool loginscreen(gamedata_t *gd, char **uname, char **password);
void updatedecor(gamedata_t *gd, object_t *player);
void updatemanager(gamedata_t *gd, object_t *o);
void cameramanage(gamedata_t *gd, object_t *player);
void updatetext(gamedata_t *gd);
void updategreeting(gamedata_t *gd, char *greeting);
bool loaddata(gamedata_t *gd);
void destroydata(gamedata_t *gd);


int main(int argc, char **argv)
{
    gamedata_t gd;
    bool status;
    char *server = "localhost";

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

    /* setup a clean world state */
    status = initworldstate(&gd.ws);
    if(status != success) {
        d_error_fatal("Worldstate init failed.\n");
        return 1;
    }

    setluaworldstate(&gd.ws);

    /* initialize message buffer */
    msgbuf_init(&gd.msgbuf, MSGBUF_SIZE);
    setluamsgbuf(&gd.msgbuf);

    /* load data from server */
    status = loaddata(&gd);
    if(status != success) {
        d_error_fatal("Data load failed.\n");
        return 1;
    }

    /* login */
    status = loginloop(&gd);
    if(status != success)
        return 0;

    /* Initialize manager */
    status = d_manager_new();
    if(status == failure)
        return failure;

    d_manager_setscrollparameters(true, 0);

    /* enter main loop */
    mainloop(&gd);

    /* close connection */
    net_close(gd.nh);

    /* deinit local */
    evsk_delete(&gd.evsk);
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


/* loginloop
 * loops around loginscreen and net_login. (alright, not really.
 * to do that, it would have to re-establish a connection with the
 * server every time a login fails. maybe add a servername field
 * in the loginscreen?) */
bool loginloop(gamedata_t *gd)
{
    char *uname = "", *password = "";
    bool status;

    status = loginscreen(gd, &uname, &password);
    if(status != success)
	return status;

    status = net_login(gd->nh, uname, password, &gd->localobj);
    return status;
}


/* loginscreen
 * displays a login screen and prompts the user for their name and
 * password.
 * FIXMEs: full of magic numbers. */
bool loginscreen(gamedata_t *gd, char **uname, char **password)
{
    /* Note: This will perhaps in the future be read from the server. */
    char *motd[4] = { "fobwart alpha-zero",
                      "Copyright 2001 by Daniel Barnes and ",
                      "                  Julian Squires.",
                      NULL, };
    d_point_t pt;
    d_rect_t r;
    int i, onfield;
    d_font_t *lshadow;
    char *field;

    /* Setup a palette and a shadow font. */
    d_raster_setpalette(&gd->raster->palette);
    lshadow = d_font_dup(gd->largefont);
    d_font_silhouette(lshadow, d_color_fromrgb(gd->raster, 0, 0, 0), 255);
    onfield = 0;

    while(1) {
	/* handle events */
        d_event_update();
	/* the user chose to escape. */
	if(d_event_ispressed(EV_QUIT))
	    return failure;

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

	/* display the cute mm2 screen decorations. */
        decor_ll_mm2screen(gd->raster);

	/* display the login prompt with mm2 window decor. */
        r.offset.x = 0; r.w = gd->raster->desc.w;
        r.offset.y = 10; r.h = gd->largefont->desc.h+4;
        decor_ll_mm2window(gd->raster, r);

        pt.x = 6;
        pt.y = r.offset.y+4;
        d_font_printf(gd->raster, lshadow, pt, LOGINPROMPT);
        pt.x--; pt.y--;
        d_font_printf(gd->raster, gd->largefont, pt, LOGINPROMPT);
        pt.x += d_font_gettextwidth(gd->largefont, LOGINPROMPT);
        pt.y += (gd->largefont->desc.h-gd->deffont->desc.h)/2;

        if(onfield == 0) {
	    /* display the field currently being edited */
            if(gd->type.buf) {
		/* clip the field at the cursor position */
		gd->type.buf[gd->type.pos] = 0;
                d_font_printf(gd->raster, gd->deffont, pt, (byte *)"%s",
                              gd->type.buf);
	    }
	    /* plot the cursor */
            pt.x += gd->type.pos*gd->deffont->desc.w;
            d_font_printf(gd->raster, gd->deffont, pt, (byte *)"\x10");
        } else
            d_font_printf(gd->raster, gd->deffont, pt, (byte *)"%s", *uname);

	/* display the password prompt with mm2-style window. */
        r.offset.x = 0; r.w = gd->raster->desc.w;
        r.offset.y += r.h+10; r.h = gd->largefont->desc.h+4;
        decor_ll_mm2window(gd->raster, r);

        pt.x = 6;
        pt.y = r.offset.y+4;
        d_font_printf(gd->raster, lshadow, pt, PASSPROMPT);
        pt.x--; pt.y--;
        d_font_printf(gd->raster, gd->largefont, pt, PASSPROMPT);
        pt.x += d_font_gettextwidth(gd->largefont, PASSPROMPT);
        pt.y += (gd->largefont->desc.h-gd->deffont->desc.h)/2;

	/* seeing as there are only two fields, this one will always
	   either be empty or in the process of being edited, so there's
	   no point drawing the finished password. */
        if(onfield == 1) {
            if(gd->type.buf)
		for(i = 0; i < gd->type.pos; i++) {
		    if(gd->type.buf[i])
			d_font_printf(gd->raster, gd->deffont, pt, (byte *)"*");
		    pt.x += gd->deffont->desc.w;
		}
	    pt.x = 6+d_font_gettextwidth(gd->largefont, PASSPROMPT);
            pt.x += gd->type.pos*gd->deffont->desc.w;
            d_font_printf(gd->raster, gd->deffont, pt, (byte *)"\x10");
	}

	/* Display the ``message of the day'', currently just the copyright
	 * message. */
        pt.x = 5;
        pt.y = gd->raster->desc.h-4*gd->largefont->desc.h;

        for(i = 0; motd[i] != NULL; i++) {
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
    return success;
}


void mainloop(gamedata_t *gd)
{
    d_timehandle_t *th;
    object_t *o;

    getobject(gd, gd->localobj);
    if(d_set_fetch(gd->ws.objs, gd->localobj, (void **)&o) != success) {
        d_error_debug("ack. couldn't fetch localobj!\n");
        return;
    }
    getroom(gd, o->location);

    gd->type.pos = 0;
    gd->type.nalloc = 0;
    gd->type.buf = NULL;
    gd->type.done = false;
    gd->quitcount = 0;
    gd->status = NULL;
    gd->readycount = 40;

    /* music is disabled atm.
    if(gd->hasaudio && gd->cursong != NULL)
        forkaudiothread(gd->cursong);
    */

    while(1) {
        th = d_time_startcount(gd->fps, false);
        d_event_update();

        /* emergency exit key */
        if(d_event_ispressed(EV_QUIT) && !gd->bounce[EV_QUIT]) {
	    break;
	}

        while(evsk_pop(&gd->evsk, NULL));
        handleinput(gd);

        net_syncevents(gd->nh, &gd->evsk);

        processevents(&gd->evsk, (void *)gd);
        updatephysics(&gd->ws);

        /* update manager/graphics */
        if(d_set_fetch(gd->ws.objs, gd->localobj, (void **)&o) != success) {
            d_error_debug("ack. couldn't fetch localobj!\n");
            return;
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

    return;
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
/*    d_memory_copy(&gd->ebar->palette, gd->curpalette,
D_NCLUTITEMS*D_BYTESPERCOLOR); */
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
