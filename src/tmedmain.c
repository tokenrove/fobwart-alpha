/*
 * tmedmain.c
 * Julian Squires <tek@wiw.org> / 2001
 */

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"

#include <dentata/time.h>
#include <dentata/manager.h>
#include <dentata/raster.h>
#include <dentata/event.h>
#include <dentata/color.h>
#include <dentata/error.h>


int tmapmode(gamedata_t *gd);
static void drawoutline(d_image_t *dst, d_point_t initpt, word w, word h,
			d_color_t c);
static void tilepalettescreen(gamedata_t *gd, d_tilemap_t *map, byte tile[2]);


int tmapmode(gamedata_t *gd)
{
    d_timehandle_t *th;
    d_point_t pt, camera;
    d_tilemap_t *map;
    room_t *room;
    packet_t p;
    word mphnd;
    bool status;
    d_color_t outline = 1, c;
    byte tile[2] = { 128, 0 };

    /* Get the tilemap on which we operate. */
    p.type = PACK_GETROOM;
    p.body.handle = 0;
    status = net_writepack(gd->nh, p);
    if(status == failure) return -1;
    status = net_readpack(gd->nh, &p);
    if(status == failure || p.type != PACK_ROOM) return -1;
    deskelroom(&p.body.room);
    map = p.body.room.map;
    d_manager_addimagelayer(p.body.room.bg, &p.body.room.bghandle, -1);
    d_manager_addtilemaplayer(map, &mphnd, 0);

    /* Reset some miscellaneous variables */
    pt.x = pt.y = 0;

    while(1) {
        th = d_time_startcount(gd->fps, false);
        d_event_update();

        /* emergency exit key */
        if(d_event_ispressed(EV_QUIT) && !isbounce(EV_QUIT)) {
	    break;
	}

        if(d_event_ispressed(EV_LEFT) && pt.x > 0) {
	    pt.x--;
	} else if(d_event_ispressed(EV_RIGHT) && pt.x < map->w-1) {
	    pt.x++;
	}

	if(d_event_ispressed(EV_UP) && pt.y > 0) {
	    pt.y--;
	} else if(d_event_ispressed(EV_DOWN) && pt.y < map->h-1) {
	    pt.y++;
	}

	if(d_event_ispressed(EV_ACT)) {
	    map->map[pt.x+pt.y*map->w] = tile[0];
	} else if(d_event_ispressed(EV_JUMP)) {
	    map->map[pt.x+pt.y*map->w] = tile[1];
	}

	if(d_event_ispressed(EV_TAB) && !isbounce(EV_TAB)) {
	    debouncecontrols();
	    /* Bring up tile palette */
	    /* widgetslide(tilepalettewidget, pt, EV_TAB); */
	    tilepalettescreen(gd, map, tile);
	    /* widgetslide(tilepalettewidget, pt, EV_TAB); */
	}

	if(d_event_ispressed(EV_CONSOLE) && !isbounce(EV_CONSOLE)) {
	    /* Push room */
	    d_error_debug("Tried to save %s (%d)\n", p.body.room.name,
			  p.body.room.handle);
	    status = net_writepack(gd->nh, p);
	    if(status == failure) return -1;
	    d_error_debug("Seemed to succeed.\n");
	    /* Push file */
/*	    savetmap(p.body.room.mapname, map);
	    p.type = PACK_FILE;
	    p.body.file.checksum = checksumfile(p.body.room.mapname); */
	}

	debouncecontrols();

	/*
	  for(i = 0; i < gd->nwidgets; i++)
	      gd->widgets[i].update(gd);
	*/
        /* update graphics */

	/* update greeting message */

        d_raster_update();
        d_time_endcount(th);
    }

    return 0;
}


void tilepalettescreen(gamedata_t *gd, d_tilemap_t *map, byte tile[2])
{
    d_timehandle_t *th;
    d_point_t pt, pos;
    d_color_t c, curcol;
    d_image_t *dbf;
    int i, cursor;

    dbf = d_image_dup(gd->raster);
    c = d_color_fromrgb(gd->raster, 220, 220, 170);

    /* Slide in */
    pos.x = gd->raster->desc.w-1;
 slidein:
    debouncecontrols();
    pos.y = 0;
    for(; pos.x > 0; pos.x-=10) {
        th = d_time_startcount(gd->fps, false);
        d_event_update();

        /* emergency exit key */
        if(d_event_ispressed(EV_QUIT) && !isbounce(EV_QUIT)) {
	    return;
	}

	if(d_event_ispressed(EV_TAB) && !isbounce(EV_TAB)) {
	    goto slideout;
	}

	debouncecontrols();

	/* Draw message box. */
	pt.x = pos.x; pt.y = pos.y;
	for(pt.y = pos.y; pt.y < gd->raster->desc.h; pt.y++)
		d_image_setpelcolor(gd->raster, pt, 16);
	for(pt.y = pos.y; pt.y < gd->raster->desc.h; pt.y++)
	    for(pt.x = pos.x+1; pt.x < gd->raster->desc.w; pt.x++) {
		d_image_setpelcolor(gd->raster, pt, c);
	    }

        d_raster_update();
        d_time_endcount(th);
    }

    curcol = 0;
    cursor = tile[0];

    /* primary loop */
    while(1) {
        th = d_time_startcount(gd->fps, false);
        d_event_update();

        /* emergency exit key */
        if(d_event_ispressed(EV_QUIT) && !isbounce(EV_QUIT)) {
	    return;
	}

	if(d_event_ispressed(EV_LEFT) && !isbounce(EV_LEFT)) {
	    do {
		cursor--;
		if(cursor < 0)
		    cursor = map->ntiles-1;
	    } while(map->tiles[cursor] == NULL);
	} else if(d_event_ispressed(EV_RIGHT) && !isbounce(EV_RIGHT)) {
	    do {
		cursor++;
		if(cursor > map->ntiles-1)
		    cursor = 0;
	    } while(map->tiles[cursor] == NULL);
	}

	if(d_event_ispressed(EV_ACT))
	    tile[0] = cursor;
	if(d_event_ispressed(EV_JUMP))
	    tile[1] = cursor;

	if(d_event_ispressed(EV_TAB) && !isbounce(EV_TAB)) {
	    break;
	}

	debouncecontrols();

	/* Draw message box. */
	pt.x = 0; pt.y = 0;
	d_image_blit(gd->raster, dbf, pt);
	for(pt.y = 0; pt.y < gd->raster->desc.h; pt.y++)
	    for(pt.x = 0; pt.x < gd->raster->desc.w; pt.x++) {
		d_image_setpelcolor(gd->raster, pt, c);
	    }

	pt.x = pt.y = 2;
	for(i = 0; i < map->ntiles; i++) {
	    if(map->tiles[i] == NULL)
		continue;

	    d_image_blit(gd->raster, map->tiles[i], pt);
	    if(i == tile[0] || i == tile[1] || i == cursor) {
		pt.x--;	pt.y--;
		drawoutline(gd->raster, pt, map->tiledesc.w+2,
			    map->tiledesc.h+2, (i == cursor)?curcol:
			    (i == tile[0])?36:52);
		pt.x++; pt.y++;
	    }

	    pt.x += map->tiledesc.w+2;
	    if(pt.x+map->tiledesc.w+2 >= gd->raster->desc.w) {
		pt.x = 2;
		pt.y += map->tiledesc.h+2;
	    }

	    if(pt.y+map->tiledesc.h+2 > gd->raster->desc.h) {
		/* FIXME, scrollbar */
		break;
	    }
	}

	curcol++;

        d_raster_update();
        d_time_endcount(th);
    }

    /* Slide out */
    pos.x = 0;
 slideout:
    debouncecontrols();
    pos.y = 0;
    for(; pos.x < gd->raster->desc.w; pos.x+=10) {
        th = d_time_startcount(gd->fps, false);
        d_event_update();

        /* emergency exit key */
        if(d_event_ispressed(EV_QUIT) && !isbounce(EV_QUIT)) {
	    return;
	}

	if(d_event_ispressed(EV_TAB) && !isbounce(EV_TAB)) {
	    goto slidein;
	}

	debouncecontrols();

	/* Draw message box. */
	pt.x = 0; pt.y = 0;
	d_image_blit(gd->raster, dbf, pt);
	pt.x = pos.x; pt.y = pos.y;
	for(pt.y = pos.y; pt.y < gd->raster->desc.h; pt.y++)
		d_image_setpelcolor(gd->raster, pt, 16);
	for(pt.y = pos.y; pt.y < gd->raster->desc.h; pt.y++)
	    for(pt.x = pos.x+1; pt.x < gd->raster->desc.w; pt.x++) {
		d_image_setpelcolor(gd->raster, pt, c);
	    }

        d_raster_update();
        d_time_endcount(th);
    }

    return;
}

/* EOF tmedmain.c */
