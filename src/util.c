/*
 * util.c ($Id$)
 * Julian Squires <tek@wiw.org> / 2001
 *
 * Various (possibly transitory) helper routines and data structures.
 */


#include "fobwart.h"

#include <dentata/memory.h>
#include <dentata/file.h>


void evsk_new(eventstack_t *evsk);
void evsk_delete(eventstack_t *evsk);
void evsk_push(eventstack_t *evsk, event_t ev);
bool evsk_top(eventstack_t *evsk, event_t *ev);
bool evsk_pop(eventstack_t *evsk, event_t *ev);

bool string_fromasciiz(string_t *dst, const char *src);
void string_delete(string_t *s);

dword checksumfile(const char *name);
void checksuminit(void);


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


/* string_fromasciiz
 * Converts an ASCII, NUL-terminated C-style string to a string_t. Stores the
 * result in dst.
 */
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


/* string_delete
 * Frees up memory used by a string_t.
 */
void string_delete(string_t *s)
{
    if(s->data != NULL)
        d_memory_delete(s->data);
    s->data = NULL;
    s->len = 0;
    return;
}


#define CRCMAGIC 0x04C11DB7

static dword crctable[256];

void checksuminit(void)
{
    int i, j;
    dword c;

    for(i = 0; i < 256; i++) {
        for(c = (i<<24), j = 8; j > 0; j--) {
            c = (c & 0x80000000) ? ((c << 1) ^ CRCMAGIC) : (c << 1);
        }
        crctable[i] = c;
    }
    return;
}


#define CRCBUFLEN 4096

dword checksumfile(const char *name)
{
    d_file_t *file;
    dword sum, len, i;
    byte data[CRCBUFLEN];

    file = d_file_open(name);
    if(!file) return 0;

    sum = 0xFFFFFFFFL;

    while(len = d_file_read(file, data, CRCBUFLEN), len > 0) {
	i = 0;
	while(len--)
	    sum = crctable[((sum>>24)^data[i++])&0xFF] ^ (sum << 8);
    }

    d_file_close(file);

    return sum^0xFFFFFFFFL;
}

/* EOF util.c */
