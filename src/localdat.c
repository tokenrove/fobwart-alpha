/* 
 * localdat.c
 * Created: Sat Jul 14 23:40:37 2001 by tek@wiw.org
 * Revised: Thu Jul 19 21:59:45 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

#include <dentata/types.h>
#include <dentata/image.h>
#include <dentata/font.h>
#include <dentata/sprite.h>
#include <dentata/pcx.h>
#include <dentata/tilemap.h>
#include <dentata/manager.h>
#include <dentata/memory.h>
#include <dentata/file.h>
#include <dentata/raster.h>
#include <dentata/error.h>
#include <dentata/sample.h>
#include <dentata/audio.h>
#include <dentata/s3m.h>
#include <dentata/set.h>

#include <lua.h>

#include "fobwart.h"
#include "fobclient.h"


extern d_image_t *ebar_new(gamedata_t *gd);

bool loaddata(gamedata_t *gd);
void destroydata(gamedata_t *gd);

bool loaddata(gamedata_t *gd)
{
    bool status;

    gd->deffont = d_font_load(DEFFONTFNAME);
    if(gd->deffont == NULL) return failure;
    gd->largefont = d_font_load(LARGEFONTFNAME);
    if(gd->largefont == NULL) return failure;

    /* load palette */
    loadpalette(LIGHTPALFNAME, &gd->light);
    loadpalette(DARKPALFNAME, &gd->dark);
    d_memory_copy(&gd->raster->palette, &gd->light,
                  D_NCLUTITEMS*D_BYTESPERCOLOR);

    status = d_manager_new();
    if(status == failure)
        return failure;

    d_manager_setscrollparameters(true, 0);

    gd->ebar = ebar_new(gd);

    if(gd->hasaudio) {
        gd->cursong = d_s3m_load(DATADIR "/mm2.s3m");
    }
    return success;
}


void destroydata(gamedata_t *gd)
{
    if(gd->hasaudio)
        d_s3m_delete(gd->cursong);

    d_image_delete(gd->ebar);
    d_manager_delete();

    d_font_delete(gd->largefont);
    gd->largefont = NULL;
    d_font_delete(gd->deffont);
    gd->deffont = NULL;
    return;
}

/* EOF localdat.c */
