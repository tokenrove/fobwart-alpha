/* 
 * audiounix.c
 * Created: Sun Jul 15 15:18:57 2001 by tek@wiw.org
 * Revised: Fri Jul 20 04:01:39 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

#include <dentata/types.h>
#include <dentata/sample.h>
#include <dentata/audio.h>
#include <dentata/s3m.h>

#include <unistd.h>


void forkaudiothread(d_s3m_t *song)
{
    d_s3mhandle_t *sh;
    bool status;
    
    if(fork() == 0) {
        sh = d_s3m_play(song);
        while(1) {
            /* update sound/music */
            status = d_s3m_update(sh);
            d_audio_update();
            if(status == true) {
                d_s3m_stop(sh);
                sh = d_s3m_play(song);
            }
        }
    }
}

/* EOF audiounix.c */
