/*
 *
 */

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"

#include <dentata/color.h>
#include <dentata/memory.h>

typedef struct ewprivate_s {
    d_image_t *raster, *ebar;
    d_palette_t *curpalette;
    word localobj;
    worldstate_t *ws;
} ewprivate_t;

enum { EBAR_W = 8, EBAR_H = 48, EBAR_MAXSLIVERS = 24 };

widget_t *ebarwidget_init(gamedata_t *gd);
static bool ebarwidget_update(widget_t *wp);
static void ebarwidget_close(widget_t *wp);


widget_t *ebarwidget_init(gamedata_t *gd)
{
    widget_t *wp;
    ewprivate_t *priv;
    d_image_t *p;
    d_rasterdescription_t mode;

    wp = d_memory_new(sizeof(widget_t));
    priv = d_memory_new(sizeof(ewprivate_t));

    mode = gd->raster->desc;
    mode.w = EBAR_W;
    mode.h = EBAR_H;

    p = d_image_new(mode);
    d_image_wipe(p, 0, 255);
    if(p->desc.paletted)
        p->palette = gd->raster->palette;

    priv->raster = gd->raster;
    priv->ebar = p;
    priv->ws = &gd->ws;
    priv->localobj = gd->localobj;

    wp->type = WIDGET_EBAR;
    wp->takesfocus = false;
    wp->private = priv;
    wp->input = NULL;
    wp->update = ebarwidget_update;
    wp->close = ebarwidget_close;

    return wp;
}


bool ebarwidget_update(widget_t *wp)
{
    ewprivate_t *priv = wp->private;
    int i, nslivers;
    d_point_t pt;
    d_color_t c[EBAR_W] = { 15, 38, 38, 32, 32, 38, 38, 15 };
    object_t *o;
    word hp, maxhp;

    d_set_fetch(priv->ws->objs, priv->localobj, (void **)&o);
    lua_getglobal(o->luastate, "hp");
    lua_getglobal(o->luastate, "maxhp");
    hp = lua_tonumber(o->luastate, 1);
    maxhp = lua_tonumber(o->luastate, 2);
    lua_pop(o->luastate, 2);

    if(maxhp < 1)
	nslivers = 0;
    else
	nslivers = hp*EBAR_MAXSLIVERS/maxhp;

    c[0] = c[7] = d_color_fromrgb(priv->raster, 0, 0, 0);
    c[1] = c[2] = c[6] = c[5] = d_color_fromrgb(priv->raster, 220, 220, 170);
    c[3] = c[4] = 32;
    c[3] = c[4] = d_color_fromrgb(priv->raster, 255, 255, 255);
    d_image_wipe(priv->ebar, c[0], 255);
    pt.x = 0;
    pt.y = (EBAR_MAXSLIVERS-nslivers)*2+1;

    for(i = 0; i < nslivers; i++, pt.y+=2) {
        for(pt.x = 0; pt.x < EBAR_W-1; pt.x++)
            d_image_setpelcolor(priv->ebar, pt, c[pt.x]);
    }

    pt.x = 8;
    pt.y = 8;
    d_image_blit(priv->raster, priv->ebar, pt);

    return false;
}


void ebarwidget_close(widget_t *wp)
{
    d_memory_delete(wp->private);
    d_memory_delete(wp);
    return;
}


/* EOF ebarwidg.c */
