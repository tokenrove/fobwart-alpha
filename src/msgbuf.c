/*
 * msgbuf.c
 * Julian Squires <tek@wiw.org> / 2001
 */

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"

#include <dentata/memory.h>
#include <dentata/event.h>
#include <dentata/color.h>


typedef struct mwprivate_s {
    d_image_t *raster;
    d_color_t activecolor, fadecolor;
    d_font_t *deffont;
    typebuf_t type;
    msgbuf_t mb;
    word localobj;
    worldstate_t *ws;
} mwprivate_t;

void msgbuf_init(msgbuf_t *mb, int size);
void msgbuf_destroy(msgbuf_t *mb);

widget_t *msgboxwidget_init(gamedata_t *gd);
static void msgboxwidget_input(widget_t *wp);
static bool msgboxwidget_update(widget_t *wp);
static void msgboxwidget_close(widget_t *wp);



void msgbuf_init(msgbuf_t *mb, int size)
{
    msgbufline_t *p;
    int i;

    p = d_memory_new(sizeof(msgbufline_t));
    memset(&p->line, 0, sizeof(p->line));

    mb->head = p;

    for(i = 0; i < size; i++) {
	p->next = d_memory_new(sizeof(msgbufline_t));
	p->next->prev = p;
	p = p->next;
	memset(&p->line, 0, sizeof(p->line));
    }

    p->next = mb->head;
    mb->head->prev = p;
    mb->current = mb->head;
    mb->bottom = mb->head;

    return;
}


/* msgbuf_destroy
 * Frees the contents of a msgbuf and its structure. */
void msgbuf_destroy(msgbuf_t *mb)
{
    msgbufline_t *p;

    p = mb->head;
    while(p->next != mb->head) {
	string_delete(&p->line);

	p = p->next;
	d_memory_delete(p->prev);
    }

    d_memory_delete(p->next);
    d_memory_delete(p);

    return;
}


widget_t *msgboxwidget_init(gamedata_t *gd)
{
    widget_t *wp;
    mwprivate_t *priv;

    wp = d_memory_new(sizeof(widget_t));
    priv = d_memory_new(sizeof(mwprivate_t));

    priv->deffont = gd->deffont;
    priv->raster = gd->raster;
    priv->activecolor = d_color_fromrgb(priv->raster, 220, 220, 170);
    priv->fadecolor = d_color_fromrgb(priv->raster, 110, 110, 85);
    priv->localobj = gd->localobj;
    priv->ws = &gd->ws;

    /* initialize message buffer */
    msgbuf_init(&priv->mb, MSGBUF_SIZE);
    setluamsgbuf(&priv->mb);

    /* Reset type buffer */
    priv->type.pos = 0;
    priv->type.nalloc = 0;
    priv->type.buf = NULL;
    priv->type.done = false;

    wp->type = WIDGET_MSGBOX;
    wp->takesfocus = true;
    wp->hasfocus = false;
    wp->private = priv;
    wp->input = msgboxwidget_input;
    wp->update = msgboxwidget_update;
    wp->close = msgboxwidget_close;

    return wp;
}


void msgboxwidget_input(widget_t *wp)
{
    mwprivate_t *priv = wp->private;
    event_t ev;
    int i;

    /* Message buffer scrolling. */
    if(d_event_ispressed(EV_PAGEDOWN) && !isbounce(EV_PAGEDOWN) &&
       priv->mb.current->next != priv->mb.bottom) {
      priv->mb.current = priv->mb.current->next;
    }

    if(d_event_ispressed(EV_PAGEUP) && !isbounce(EV_PAGEUP) &&
	priv->mb.current != priv->mb.head) {
	priv->mb.current = priv->mb.current->prev;
    }

    i = handletextinput(&priv->type);
    if(i == 1 && priv->type.pos > 0) {
	ev.verb = VERB_TALK;
	ev.subject = priv->localobj;
	ev.auxdata = d_memory_new(priv->type.pos+1);
	ev.auxlen = priv->type.pos+1;
	d_memory_copy(ev.auxdata, priv->type.buf, priv->type.pos+1);
	evsk_push(&priv->ws->evsk, ev);
	d_memory_set(priv->type.buf, 0, priv->type.nalloc);
	priv->type.pos = 0;
    }

    return;
}


bool msgboxwidget_update(widget_t *wp)
{
    mwprivate_t *priv = wp->private;
    d_point_t pt;
    msgbufline_t *p;

    /* Draw message box. */
    pt.x = 0;
    pt.y = 200;
    for(pt.y = 200; pt.y < priv->raster->desc.h; pt.y++)
        for(pt.x = 0; pt.x < priv->raster->desc.w; pt.x++) {
	    d_image_setpelcolor(priv->raster, pt,
				wp->hasfocus ? priv->activecolor:priv->fadecolor);
        }

    /* update text */
    pt.y = 202; pt.x = 2;
    if(wp->hasfocus) {
	if(priv->type.buf) {
	    priv->type.buf[priv->type.pos] = 0;
	    d_font_printf(priv->raster, priv->deffont, pt, (byte *)"%s",
			  priv->type.buf);
	}
	pt.x = 2+priv->type.pos*priv->deffont->desc.w;
	d_font_printf(priv->raster, priv->deffont, pt, (byte *)"\x10");
    }

    pt.x = 2; pt.y += priv->deffont->desc.h+1;
    for(p = priv->mb.current; p->line.data && p != priv->mb.bottom &&
	    pt.y < priv->raster->desc.h-priv->deffont->desc.h+1; p = p->next) {
	d_font_printf(priv->raster, priv->deffont, pt, (byte *)"%s", p->line.data);

	pt.y += priv->deffont->desc.h+1;
    }

    return false;
}


void msgboxwidget_close(widget_t *wp)
{
    mwprivate_t *priv = wp->private;

    msgbuf_destroy(&priv->mb);
    d_memory_delete(priv);
    d_memory_delete(wp);
    return;
}

/* EOF msgbuf.c */
