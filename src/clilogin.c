/*
 * clilogin.c
 * Julian Squires <tek@wiw.org> / 2001
 */

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"

#include <dentata/raster.h>
#include <dentata/color.h>
#include <dentata/memory.h>
#include <dentata/event.h>


bool loginloop(gamedata_t *gd);
bool loginscreen(gamedata_t *gd, char **uname, char **password);


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
    typebuf_t type;

    type.pos = type.nalloc = 0;
    type.buf = NULL; type.done = false;

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

        i = handletextinput(&type);
        if(i == 1) {
            if(type.pos > 0) {
                field = d_memory_new(type.pos+1);
                d_memory_copy(field, type.buf, type.pos+1);
                d_memory_set(type.buf, 0, type.nalloc);
                type.pos = 0;
                if(onfield == 0) {
                    *uname = field;
                    onfield++;
                } else if(onfield == 1) {
                    *password = field;
                    break;
                }
            }
        }
	debouncecontrols();

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
            if(type.buf) {
		/* clip the field at the cursor position */
		type.buf[type.pos] = 0;
                d_font_printf(gd->raster, gd->deffont, pt, (byte *)"%s",
                              type.buf);
	    }
	    /* plot the cursor */
            pt.x += type.pos*gd->deffont->desc.w;
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
            if(type.buf)
		for(i = 0; i < type.pos; i++) {
		    if(type.buf[i])
			d_font_printf(gd->raster, gd->deffont, pt, (byte *)"*");
		    pt.x += gd->deffont->desc.w;
		}
	    pt.x = 6+d_font_gettextwidth(gd->largefont, PASSPROMPT);
            pt.x += type.pos*gd->deffont->desc.w;
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


/* EOF clilogin.c */
