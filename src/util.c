/*
 * util.c ($Id$)
 * Julian Squires <tek@wiw.org> / 2001
 *
 * Various (possibly transitory) helper routines and data structures.
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

#include <lua.h>

#include "fobwart.h"


void evsk_new(eventstack_t *evsk);
void evsk_delete(eventstack_t *evsk);
void evsk_push(eventstack_t *evsk, event_t ev);
bool evsk_top(eventstack_t *evsk, event_t *ev);
bool evsk_pop(eventstack_t *evsk, event_t *ev);

bool string_fromasciiz(string_t *dst, const char *src);
void string_delete(string_t *s);


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


bool evsk_pop(eventstack_t *evsk, event_t *ev)
{
    if(evsk->top == 0)
        return false;
    if(ev != NULL)
        *ev = evsk->events[evsk->top-1];
    evsk->top--;
    return true;
}


bool string_fromasciiz(string_t *dst, const char *src)
{
    dst->len = strlen(src)+1;
    if(dst->len == 1) {
        dst->len = 0;
        dst->data = NULL;
        return success;
    }
    dst->data = d_memory_new(dst->len);
    if(dst->data == NULL) return failure;
    d_memory_copy(dst->data, src, dst->len);
    return success;
}


void string_delete(string_t *s)
{
    if(s->data != NULL)
        d_memory_delete(s->data);
    s->data = NULL;
    s->len = 0;
    return;
}

/* EOF util.c */
