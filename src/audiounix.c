/* 
 * audiounix.c
 * Created: Sun Jul 15 15:18:57 2001 by tek@wiw.org
 * Revised: Thu Jul 19 19:25:54 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

#include <dentata/types.h>
#include <dentata/image.h>
#include <dentata/error.h>
#include <dentata/time.h>
#include <dentata/raster.h>
#include <dentata/event.h>
#include <dentata/font.h>
#include <dentata/sprite.h>
#include <dentata/tilemap.h>
#include <dentata/manager.h>
#include <dentata/sample.h>
#include <dentata/audio.h>
#include <dentata/s3m.h>
#include <dentata/set.h>

#include <unistd.h>
#include <lua.h>

#include "fobwart.h"
#include "fobclient.h"


void forkaudiothread(gamedata_t *gd)
{
    d_s3mhandle_t *sh;
    bool status;
    
    if(fork() == 0) {
        sh = d_s3m_play(gd->cursong);
        while(1) {
            /* update sound/music */
            status = d_s3m_update(sh);
            d_audio_update();
            if(status == true) {
                d_s3m_stop(sh);
                sh = d_s3m_play(gd->cursong);
            }
        }
    }
}

/* EOF audiounix.c */
