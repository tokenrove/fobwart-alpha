/* 
 * decor.c
 * Created: Sun Jul 15 05:52:57 2001 by tek@wiw.org
 * Revised: Thu Jul 19 19:25:40 2001 by tek@wiw.org
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
#include "fobclient.h"

#define EBAR_W 8
#define EBAR_H 48


d_image_t *ebar_new(gamedata_t *gd);
void ebar_draw(d_image_t *bar, int nslivers);


d_image_t *ebar_new(gamedata_t *gd)
{
    d_image_t *p;
    d_rasterdescription_t mode;

    mode = gd->raster->desc;
    mode.w = EBAR_W;
    mode.h = EBAR_H;

    p = d_image_new(mode);
    d_image_wipe(p, 0, 255);
    if(p->desc.paletted)
        p->palette = gd->raster->palette;
    return p;
}


void ebar_draw(d_image_t *bar, int nslivers)
{
    int i;
    d_point_t pt;
    d_color_t c[EBAR_W] = { 15, 38, 38, 32, 32, 38, 38, 15 };

    c[0] = c[7] = d_color_fromrgb(bar, 0, 0, 0);
    c[1] = c[2] = c[6] = c[5] = d_color_fromrgb(bar, 255, 255, 190);
    c[3] = c[4] = d_color_fromrgb(bar, 255, 255, 255);
    d_image_wipe(bar, c[0], 255);
    pt.x = 0;
    pt.y = (EBAR_MAXSLIVERS-nslivers)*2+1;

    for(i = 0; i < nslivers; i++, pt.y+=2) {
        for(pt.x = 0; pt.x < EBAR_W-1; pt.x++)
            d_image_setpelcolor(bar, pt, c[pt.x]);
    }
}

/* EOF decor.c */
