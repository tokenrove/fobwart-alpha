/*
 * mapeditw.c
 * Julian Squires <tek@wiw.org> / 2001
 */

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"

#include <dentata/memory.h>
#include <dentata/color.h>
#include <dentata/event.h>
#include <dentata/manager.h>

typedef struct meprivate_s {
    gamedata_t *gd;
    d_image_t *raster;
    d_font_t *deffont;
    nethandle_t *nh;
    worldstate_t *ws;
    room_t *room;
    d_point_t cursor;
    d_color_t boxcolor;
    byte tile[2], outline;
} meprivate_t;

widget_t *mapeditwidget_init(gamedata_t *gd);
static void mapeditwidget_input(widget_t *wp);
static void mapeditwidget_close(widget_t *wp);
static bool mapeditwidget_update(widget_t *wp);
static bool loadroom(meprivate_t *priv, roomhandle_t handle);
static bool saveroom(meprivate_t *priv);
static void drawoutline(d_image_t *dst, d_point_t initpt, word w, word h,
			d_color_t c);


widget_t *mapeditwidget_init(gamedata_t *gd)
{
    widget_t *wp;
    meprivate_t *priv;
    object_t *o;

    wp = d_memory_new(sizeof(widget_t));
    priv = d_memory_new(sizeof(meprivate_t));
    priv->raster = gd->raster;
    priv->deffont = gd->deffont;
    priv->gd = gd;
    priv->ws = &gd->ws;
    priv->boxcolor = d_color_fromrgb(priv->raster, 220, 220, 170);
    priv->outline = 0;
    priv->cursor.x = 0;
    priv->cursor.y = 0;
    priv->tile[0] = 0;
    priv->tile[1] = 128;

    getobject(gd, gd->localobj);
    d_set_fetch(priv->ws->objs, gd->localobj, (void **)&o);
    loadroom(priv, o->location);

    wp->type = WIDGET_MAPEDIT;
    wp->takesfocus = true;
    wp->private = priv;
    wp->input = mapeditwidget_input;
    wp->update = mapeditwidget_update;
    wp->close = mapeditwidget_close;

    debouncecontrols();

    return wp;
}


/* mapeditwidget_input
 * Process map editing widget input. */
void mapeditwidget_input(widget_t *wp)
{
    meprivate_t *priv = wp->private;
    d_tilemap_t *map;

    map = priv->room->map;

    if(d_event_ispressed(EV_LEFT) && priv->cursor.x > 0) {
	priv->cursor.x--;
    } else if(d_event_ispressed(EV_RIGHT) && priv->cursor.x < map->w-1) {
	priv->cursor.x++;
    }

    if(d_event_ispressed(EV_UP) && priv->cursor.y > 0) {
	priv->cursor.y--;
    } else if(d_event_ispressed(EV_DOWN) && priv->cursor.y < map->h-1) {
	priv->cursor.y++;
    }
    
    if(d_event_ispressed(EV_ACT)) {
	map->map[priv->cursor.x+priv->cursor.y*map->w] = priv->tile[0];
    } else if(d_event_ispressed(EV_JUMP)) {
	map->map[priv->cursor.x+priv->cursor.y*map->w] = priv->tile[1];
    }

    if(d_event_ispressed(EV_CONSOLE) && !isbounce(EV_CONSOLE)) {
	saveroom(priv);
    }

    return;
}


void mapeditwidget_close(widget_t *wp)
{
    meprivate_t *priv = wp->private;

    d_memory_delete(priv);
    wp->private = NULL;
    d_memory_delete(wp);
    return;
}


bool mapeditwidget_update(widget_t *wp)
{
    meprivate_t *priv = wp->private;
    d_point_t pt;
    d_tilemap_t *map;

    map = priv->room->map;
    pt.x = priv->cursor.x*map->tiledesc.w-priv->raster->desc.w/2;
    if(pt.x < 0)
	pt.x = 0;
    else if(pt.x+priv->raster->desc.w > map->w*map->tiledesc.w)
	pt.x = map->w*map->tiledesc.w-priv->raster->desc.w;

    pt.y = priv->cursor.y*map->tiledesc.h-(priv->raster->desc.h-40)/2;
    if(pt.y < 0)
	pt.y = 0;
    else if(pt.y+priv->raster->desc.h-40 > map->w*map->tiledesc.h)
	pt.y = map->h*map->tiledesc.h-priv->raster->desc.h-40;

    d_image_wipe(priv->raster, 16, 255);

    d_manager_jump(pt.x, pt.y);
    d_manager_draw(priv->raster);

    pt.x = priv->cursor.x*map->tiledesc.w-pt.x;
    pt.y = priv->cursor.y*map->tiledesc.h-pt.y;
    d_image_blit(priv->raster, map->tiles[priv->tile[0]], pt);

    priv->outline++;
    drawoutline(priv->raster, pt, map->tiledesc.w, map->tiledesc.h,
		priv->outline);

    /* update decor */
    /* update text */
    /* Draw message box. */
    pt.x = 0;
    pt.y = 200;
    for(pt.y = 200; pt.y < priv->raster->desc.h; pt.y++)
	for(pt.x = 0; pt.x < priv->raster->desc.w; pt.x++) {
	    d_image_setpelcolor(priv->raster, pt, priv->boxcolor);
	}

    pt.x = 2;
    pt.y = 202;
    d_font_printf(priv->raster, priv->deffont, pt, "%s (%dx%d)",
		  priv->room->name, map->w, map->h);

    pt.x = priv->raster->desc.w-4-map->tiledesc.w;
    d_image_blit(priv->raster, map->tiles[priv->tile[0]], pt);
    pt.y += map->tiledesc.h+2;
    pt.x += 2;
    d_image_blit(priv->raster, map->tiles[priv->tile[1]], pt);

    return false;
}


bool loadroom(meprivate_t *priv, roomhandle_t handle)
{
    packet_t p;
    bool status;

    /* Get the tilemap on which we operate. */
    p.type = PACK_GETROOM;
    p.body.handle = 0;
    status = net_writepack(priv->gd->nh, p);
    if(status == failure) return failure;
    status = net_readpack(priv->gd->nh, &p);
    if(status == failure || p.type != PACK_ROOM) return failure;
    priv->room = d_memory_new(sizeof(room_t));
    d_memory_copy(priv->room, &p.body.room, sizeof(room_t));
    deskelroom(priv->room);

    d_memory_copy(&priv->room->map->tiledesc,
		  &priv->room->map->tiles[0]->desc,
		  sizeof(priv->room->map->tiledesc));

    d_manager_wipelayers();
    d_manager_wipesprites();
    d_manager_addimagelayer(priv->room->bg, &priv->room->bghandle, -1);
    d_manager_addtilemaplayer(priv->room->map, &priv->room->tmhandle, 0);

    return success;
}


bool saveroom(meprivate_t *priv)
{
    packet_t p;
    bool status;

    p.type = PACK_ROOM;
    d_memory_copy(&p.body.room, priv->room, sizeof(p.body.room));

    d_error_debug("Tried to save %s (%d)\n", p.body.room.name,
		  p.body.room.handle);
    status = net_writepack(priv->gd->nh, p);
    if(status != success) return failure;
    d_error_debug("Seemed to succeed.\n");

    return success;
}


void drawoutline(d_image_t *dst, d_point_t initpt, word w, word h, d_color_t c)
{
    d_point_t pt;

    for(pt.y = initpt.y; pt.y < initpt.y+h; pt.y++) {
	pt.x = initpt.x;
	d_image_setpelcolor(dst, pt, c);
	pt.x = initpt.x+w-1;
	d_image_setpelcolor(dst, pt, c);
    }
    for(pt.x = initpt.x; pt.x < initpt.x+w; pt.x++) {
	pt.y = initpt.y;
	d_image_setpelcolor(dst, pt, c);
	pt.y = initpt.y+h-1;
	d_image_setpelcolor(dst, pt, c);
    }

    return;
}


/* EOF mapeditw.c */
