/*
 * debugcon.c - superuser debugging console
 * Julian Squires <tek@wiw.org> / 2001
 */

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"

#include <dentata/memory.h>
#include <dentata/error.h>
#include <dentata/event.h>

#include <stdlib.h>
#include <string.h>


typedef struct dcprivate_s {
    gamedata_t *gd;
    typebuf_t type;
    bool quit;
} dcprivate_t;


widget_t *debugconsole_init(gamedata_t *gd);
static void debugconsole_input(widget_t *wp);
static bool debugconsole_update(widget_t *wp);
static void debugconsole_close(widget_t *wp);


widget_t *debugconsole_init(gamedata_t *gd)
{
    widget_t *wp;
    dcprivate_t *priv;

    wp = d_memory_new(sizeof(widget_t));
    priv = d_memory_new(sizeof(dcprivate_t));

    priv->gd = gd;
    priv->quit = false;

    /* Reset type buffer */
    priv->type.pos = 0;
    priv->type.nalloc = 0;
    priv->type.buf = NULL;
    priv->type.done = false;

    wp->type = WIDGET_CONSOLE;
    wp->takesfocus = true;
    wp->hasfocus = true;
    wp->private = priv;
    wp->input = debugconsole_input;
    wp->update = debugconsole_update;
    wp->close = debugconsole_close;

    return wp;
}


void debugconsole_input(widget_t *wp)
{
    dcprivate_t *priv = wp->private;
    char *p, *q;
    int i;
    roomhandle_t roomkey;
    event_t ev;

    if(d_event_ispressed(EV_CONSOLE) && !isbounce(EV_CONSOLE)) {
	priv->quit = true;
	return;
    }

    i = handletextinput(&priv->type);
    if(i == 1 && priv->type.pos > 0) {
	p = d_memory_new(priv->type.nalloc);
	d_memory_copy(p, priv->type.buf, priv->type.pos+1);

	q = strtok(p, " ");
	if(!strcmp(q, "quit")) {
	    d_error_fatal(__FUNCTION__": user requested quit.\n");

	} else if(!strcmp(q, "goto")) {
	    q = strtok(NULL, " ");
	    if(q != NULL) {
		d_error_debug(__FUNCTION__": user requested goto: ``%s''\n", q);
		ev.verb = VERB_EXIT;
		ev.subject = priv->gd->localobj;
		ev.auxlen = sizeof(roomhandle_t);
		roomkey = atoi(q);
		d_memory_copy(ev.auxdata, &roomkey, sizeof(roomhandle_t));
		evsk_push(&priv->gd->ws.evsk, ev);
	    }
	}

	d_memory_delete(p);
	d_memory_set(priv->type.buf, 0, priv->type.nalloc);
	priv->type.pos = 0;
    }

    return;
}


bool debugconsole_update(widget_t *wp)
{
    dcprivate_t *priv = wp->private;
    d_point_t pt;
    d_rect_t r;

    r.offset.x = 0; r.w = priv->gd->raster->desc.w;
    r.h = 2*priv->gd->deffont->desc.h+6;
    r.offset.y = priv->gd->raster->desc.h/2-r.h/2;
    decor_ll_mm2window(priv->gd->raster, r);

    pt.x = 6;
    pt.y = r.offset.y+8;

    if(priv->type.buf) {
	priv->type.buf[priv->type.pos] = 0;
	d_font_printf(priv->gd->raster, priv->gd->deffont, pt, (byte *)"> %s\x10",
		      priv->type.buf);
    } else {
	d_font_printf(priv->gd->raster, priv->gd->deffont, pt, (byte *)"> \x10");
    }

    return priv->quit;
}


void debugconsole_close(widget_t *wp)
{
    dcprivate_t *priv = wp->private;

    if(priv->type.buf) d_memory_delete(priv->type.buf);
    d_memory_delete(priv);
    d_memory_delete(wp);
    return;
}

/* EOF debugcon.c */
