/*
 * greetwdg.c -- greeting widget
 * Julian Squires <tek@wiw.org> / 2001
 */

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"

#include <dentata/memory.h>


typedef struct gwprivate_s {
    d_image_t *raster;
    d_font_t *font;
    word counter;
    char *msg;
} gwprivate_t;

widget_t *greetingwidget_init(gamedata_t *gd, char *msg);
static bool greetingwidget_update(widget_t *wp);
static void greetingwidget_close(widget_t *wp);


widget_t *greetingwidget_init(gamedata_t *gd, char *msg)
{
    widget_t *wp;
    gwprivate_t *priv;

    wp = d_memory_new(sizeof(widget_t));
    priv = d_memory_new(sizeof(gwprivate_t));

    priv->raster = gd->raster;
    priv->font = gd->largefont;
    priv->msg = msg;
    priv->counter = 50;

    wp->type = WIDGET_GREETING;
    wp->takesfocus = false;
    wp->private = priv;
    wp->input = NULL;
    wp->update = greetingwidget_update;
    wp->close = greetingwidget_close;

    return wp;
}


/* greetingwidget_update
 * display a flashing message in the center of the screen, while
 * the ``readycount'' is still valid. (it's reset on a new game and
 * on a death) */
bool greetingwidget_update(widget_t *wp)
{
    gwprivate_t *priv = wp->private;
    d_point_t pt;

    if(priv->counter > 0) {
	pt.x = (priv->raster->desc.w-
		d_font_gettextwidth(priv->font,
				    (byte *)"%s", priv->msg))/2;
	pt.y = (priv->raster->desc.h-priv->font->desc.h)/2-20;
	priv->counter--;

	if(priv->counter%8 > 4)
	    d_font_printf(priv->raster, priv->font, pt, (byte *)"%s",
			  priv->msg);

	return false;
    } else {
	return true;
    }
}


void greetingwidget_close(widget_t *wp)
{
    d_memory_delete(wp->private);
    d_memory_delete(wp);
    return;
}

/* EOF greetwdg.c */
