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


/* Low-level decor routines */
void decor_ll_mm2screen(d_image_t *bg);
void decor_ll_mm2window(d_image_t *bg, d_rect_t r);


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
