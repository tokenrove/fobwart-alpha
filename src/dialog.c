/*
 *
 */

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"

#include <dentata/memory.h>
#include <dentata/event.h>


typedef struct ynprivate_s {
    d_image_t *raster;
    d_font_t *font;
    char *msg;
    bool result, quit;
} ynprivate_t;


widget_t *yesnodialog_init(gamedata_t *gd, char *msg, int type);
bool yesnodialog_return(widget_t *wp);
static void yesnodialog_input(widget_t *wp);
static bool yesnodialog_update(widget_t *wp);
static void anydialog_close(widget_t *wp);


widget_t *yesnodialog_init(gamedata_t *gd, char *msg, int type)
{
    widget_t *wp;
    ynprivate_t *priv;

    wp = d_memory_new(sizeof(widget_t));
    priv = d_memory_new(sizeof(ynprivate_t));

    priv->raster = gd->raster;
    priv->font = gd->largefont;
    priv->msg = msg;
    priv->result = false;
    priv->quit = false;

    wp->type = type;
    wp->takesfocus = true;
    wp->hasfocus = true;
    wp->private = priv;
    wp->input = yesnodialog_input;
    wp->update = yesnodialog_update;
    wp->close = anydialog_close;

    return wp;
}


bool yesnodialog_return(widget_t *wp)
{
    ynprivate_t *priv = wp->private;

    return priv->result;
}


void yesnodialog_input(widget_t *wp)
{
    ynprivate_t *priv = wp->private;

    if(d_event_ispressed(EV_LEFT))
	priv->result = true;
    else if(d_event_ispressed(EV_RIGHT))
	priv->result = false;
    else if(d_event_ispressed(EV_ENTER))
	priv->quit = true;

    return;
}


bool yesnodialog_update(widget_t *wp)
{
    ynprivate_t *priv = wp->private;
    d_point_t pt;
    d_rect_t r;

    r.offset.x = 0; r.w = priv->raster->desc.w;
    r.h = 2*priv->font->desc.h+6;
    r.offset.y = priv->raster->desc.h/2-r.h/2;
    decor_ll_mm2window(priv->raster, r);

    pt.x = 6;
    pt.y = r.offset.y+4;
    d_font_printf(priv->raster, priv->font, pt, (byte *)"%s", priv->msg);

    pt.y += priv->font->desc.h+2;
    d_font_printf(priv->raster, priv->font, pt, (byte *)"%c%s -- %c%s",
		  priv->result?0x10:' ', "YES", priv->result?' ':0x10, "NO");

    return priv->quit;
}


void anydialog_close(widget_t *wp)
{
    d_memory_delete(wp->private);
    d_memory_delete(wp);
    return;
}

/* EOF dialog.c */
