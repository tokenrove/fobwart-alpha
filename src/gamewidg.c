/*
 *
 */

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"

#include <dentata/error.h>
#include <dentata/memory.h>
#include <dentata/manager.h>
#include <dentata/raster.h>
#include <dentata/random.h>
#include <dentata/util.h>
#include <dentata/event.h>


typedef struct gwprivate_s {
    d_image_t *raster;
    d_palette_t *curpalette;
    worldstate_t *ws;
    word localobj;
} gwprivate_t;


widget_t *gamewidget_init(gamedata_t *gd);
static bool gamewidget_update(widget_t *wp);
static void gamewidget_input(widget_t *wp);
static void gamewidget_close(widget_t *wp);
static void cameramanage(gwprivate_t *priv, object_t *player);


/* gamewidget_init
 * Creates a new gamewidget. */
widget_t *gamewidget_init(gamedata_t *gd)
{
    widget_t *wp;
    gwprivate_t *priv;

    wp = d_memory_new(sizeof(widget_t));
    priv = d_memory_new(sizeof(gwprivate_t));
    priv->raster = gd->raster;
    priv->curpalette = &gd->raster->palette;
    priv->ws = &gd->ws;
    priv->localobj = gd->localobj;

    wp->type = WIDGET_GAME;
    wp->takesfocus = true;
    wp->private = priv;
    wp->input = gamewidget_input;
    wp->update = gamewidget_update;
    wp->close = gamewidget_close;

    debouncecontrols();

    return wp;
}


/* gamewidget_input
 * Process gamewidget input. */
void gamewidget_input(widget_t *wp)
{
    gwprivate_t *priv = wp->private;
    room_t *room;
    object_t *o;
    event_t ev;
    byte *d;

    d_set_fetch(priv->ws->objs, priv->localobj, (void **)&o);
    d_set_fetch(priv->ws->rooms, o->location, (void **)&room);

    if(d_event_ispressed(EV_ACT) && !isbounce(EV_ACT)) {
        ev.subject = priv->localobj;
        ev.verb = VERB_ACT;
        ev.auxdata = d = d_memory_new(1);
        d[0] = 1;
        ev.auxlen = 1;
        evsk_push(&priv->ws->evsk, ev);
    }

    if(d_event_ispressed(EV_JUMP) && !isbounce(EV_JUMP)) {
        ev.subject = priv->localobj;
        ev.verb = VERB_ACT;
        ev.auxdata = d = d_memory_new(1);
        d[0] = 2;
        ev.auxlen = 1;
        evsk_push(&priv->ws->evsk, ev);
    }

    if(d_event_ispressed(EV_RIGHT)) {
        ev.subject = priv->localobj;
        ev.verb = VERB_RIGHT;
        ev.auxdata = NULL;
        ev.auxlen = 0;
        evsk_push(&priv->ws->evsk, ev);
    } else if(d_event_ispressed(EV_LEFT)) {
        ev.subject = priv->localobj;
        ev.verb = VERB_LEFT;
        ev.auxdata = NULL;
        ev.auxlen = 0;
        evsk_push(&priv->ws->evsk, ev);
    }

    if(d_event_ispressed(EV_UP)) {
        ev.subject = priv->localobj;
        ev.verb = VERB_UP;
        ev.auxdata = NULL;
        ev.auxlen = 0;
        evsk_push(&priv->ws->evsk, ev);
    } else if(d_event_ispressed(EV_DOWN)) {
        ev.subject = priv->localobj;
        ev.verb = VERB_DOWN;
        ev.auxdata = NULL;
        ev.auxlen = 0;
        evsk_push(&priv->ws->evsk, ev);
    }

    if(d_event_ispressed(EV_BACKSPACE) && !isbounce(EV_BACKSPACE))
	room->islit ^= 1;

    return;
}


void gamewidget_close(widget_t *wp)
{
    gwprivate_t *priv = wp->private;

    d_memory_delete(priv);
    wp->private = NULL;
    d_memory_delete(wp);
    return;
}


bool gamewidget_update(widget_t *wp)
{
    gwprivate_t *priv = wp->private;
    room_t *room;
    d_iterator_t it;
    dword key;
    object_t *o, *o2;

    /* update manager/graphics */
    if(d_set_fetch(priv->ws->objs, priv->localobj, (void **)&o) != success) {
	d_error_debug("ack. couldn't fetch localobj!\n");
	return true;
    }

    d_set_fetch(priv->ws->rooms, o->location, (void **)&room);
    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, priv->ws->objs), key != D_SET_INVALIDKEY) {
        d_set_fetch(priv->ws->objs, key, (void **)&o2);
        d_manager_jumpsprite(o2->sphandle, o2->x, o2->y);
    }
    cameramanage(priv, o);
    d_manager_draw(priv->raster);
    d_raster_setpalette(priv->curpalette);
    return false;
}


void cameramanage(gwprivate_t *priv, object_t *player)
{
    int x, y;
    d_rect_t r;

    r = d_manager_getvirtualrect();
    x = player->x-priv->raster->desc.w/2;
    y = player->y-priv->raster->desc.h/2;
    if(x+priv->raster->desc.w > r.w) x = r.w-priv->raster->desc.w;
    /* Note: height of text window */
    if(y+(priv->raster->desc.h-40) > r.h) y = r.h-(priv->raster->desc.h-40);
    if(x < 0) x = 0;
    if(y < 0) y = 0;
    d_manager_jump(x, y);
    return;
}


/* EOF gamewidg.c */
