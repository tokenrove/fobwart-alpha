/* 
 * decor.c
 * Created: Sun Jul 15 05:52:57 2001 by tek@wiw.org
 * Revised: Fri Jul 20 04:01:31 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

#include <dentata/types.h>
#include <dentata/image.h>
#include <dentata/error.h>
#include <dentata/time.h>
#include <dentata/font.h>
#include <dentata/sprite.h>
#include <dentata/tilemap.h>
#include <dentata/color.h>
#include <dentata/raster.h>
#include <dentata/sample.h>
#include <dentata/audio.h>
#include <dentata/s3m.h>
#include <dentata/set.h>

#include <lua.h>

#include "fobwart.h"


enum { EBAR_W = 8, EBAR_H = 48, EBAR_MAXSLIVERS = 24 };

d_image_t *ebar_new(d_image_t *raster);
void ebar_draw(d_image_t *bar, d_color_t primary, int a, int b);

/* Low-level decor routines */
void decor_ll_mm2screen(d_image_t *bg);
void decor_ll_mm2window(d_image_t *bg, d_rect_t r);


d_image_t *ebar_new(d_image_t *raster)
{
    d_image_t *p;
    d_rasterdescription_t mode;

    mode = raster->desc;
    mode.w = EBAR_W;
    mode.h = EBAR_H;

    p = d_image_new(mode);
    d_image_wipe(p, 0, 255);
    if(p->desc.paletted)
        p->palette = raster->palette;
    return p;
}


void ebar_draw(d_image_t *bar, d_color_t primary, int a, int b)
{
    int i, nslivers;
    d_point_t pt;
    d_color_t c[EBAR_W] = { 15, 38, 38, 32, 32, 38, 38, 15 };

    nslivers = a*EBAR_MAXSLIVERS/b;
    c[0] = c[7] = d_color_fromrgb(bar, 0, 0, 0);
    c[1] = c[2] = c[6] = c[5] = primary;
    c[3] = c[4] = d_color_fromrgb(bar, 255, 255, 255);
    d_image_wipe(bar, c[0], 255);
    pt.x = 0;
    pt.y = (EBAR_MAXSLIVERS-nslivers)*2+1;

    for(i = 0; i < nslivers; i++, pt.y+=2) {
        for(pt.x = 0; pt.x < EBAR_W-1; pt.x++)
            d_image_setpelcolor(bar, pt, c[pt.x]);
    }
}


void decor_ll_mm2screen(d_image_t *bg)
{
    d_color_t c[] = { 0 };

    c[0] = d_color_fromrgb(bg, 0, 20, 255);

    d_image_wipe(bg, c[0], 255);
    return;
}


void decor_ll_mm2window(d_image_t *bg, d_rect_t r)
{
    d_point_t pt;
    d_color_t c[] = { 0, 1, 2 };

    c[0] = d_color_fromrgb(bg, 0, 20, 255);
    c[1] = d_color_fromrgb(bg, 0, 255, 255);
    c[2] = d_color_fromrgb(bg, 255, 255, 255);

    pt.y = r.offset.y;
    for(pt.x = r.offset.x; pt.x < r.offset.x+r.w; pt.x++)
        d_image_setpelcolor(bg, pt, c[1]);
    pt.y += 2;
    for(pt.x = r.offset.x; pt.x < r.offset.x+r.w; pt.x++)
        d_image_setpelcolor(bg, pt, c[1]);
    pt.y += 2;
    for(; pt.y < r.offset.y+r.h; pt.y++)
        for(pt.x = r.offset.x; pt.x < r.offset.x+r.w; pt.x++)
            d_image_setpelcolor(bg, pt, c[1]);
    pt.y++;
    for(pt.x = r.offset.x; pt.x < r.offset.x+r.w; pt.x++)
        d_image_setpelcolor(bg, pt, c[1]);
    pt.y += 2;
    for(pt.x = r.offset.x; pt.x < r.offset.x+r.w; pt.x++)
        d_image_setpelcolor(bg, pt, c[1]);

    return;
}

/* EOF decor.c */
