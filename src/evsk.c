/* 
 * evsk.c
 * Created: Tue Jul 17 05:16:17 2001 by tek@wiw.org
 * Revised: Fri Jul 20 04:01:51 2001 by tek@wiw.org
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
#include <dentata/color.h>
#include <dentata/memory.h>

#include <sys/types.h>
#include <limits.h>
#include <db.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>

#include <lua.h>

#include "fobwart.h"

void evsk_new(eventstack_t *evsk);
void evsk_delete(eventstack_t *evsk);
void evsk_push(eventstack_t *evsk, event_t ev);
bool evsk_top(eventstack_t *evsk, event_t *ev);
bool evsk_pop(eventstack_t *evsk);

void evsk_new(eventstack_t *evsk)
{
    evsk->top = 0;
    evsk->nalloc = 256;
    evsk->events = d_memory_new(evsk->nalloc*sizeof(event_t));
    return;
}

void evsk_delete(eventstack_t *evsk)
{
    if(evsk->events != NULL)
        d_memory_delete(evsk->events);
    return;
}

void evsk_push(eventstack_t *evsk, event_t ev)
{
    evsk->top++;
    if(evsk->top == evsk->nalloc) {
        evsk->nalloc *= 2;
        evsk->events = d_memory_resize(evsk->events,
                                       evsk->nalloc*sizeof(event_t));
    }
    evsk->events[evsk->top-1] = ev;
    return;
}

bool evsk_top(eventstack_t *evsk, event_t *ev)
{
    if(evsk->top == 0)
        return false;
    if(ev != NULL)
        *ev = evsk->events[evsk->top-1];
    return true;
}

bool evsk_pop(eventstack_t *evsk)
{
    if(evsk->top == 0)
        return false;
    evsk->top--;
    return true;
}

/* EOF evsk.c */
