/*
 * msgbuf.c
 * Julian Squires <tek@wiw.org> / 2001
 */

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"

#include <dentata/memory.h>


void msgbuf_init(msgbuf_t *mb, int size);
void msgbuf_destroy(msgbuf_t *mb);


void msgbuf_init(msgbuf_t *mb, int size)
{
    msgbufline_t *p;
    int i;

    p = d_memory_new(sizeof(msgbufline_t));
    memset(&p->line, 0, sizeof(p->line));

    mb->head = p;

    for(i = 0; i < size; i++) {
	p->next = d_memory_new(sizeof(msgbufline_t));
	p->next->prev = p;
	p = p->next;
	memset(&p->line, 0, sizeof(p->line));
    }

    p->next = mb->head;
    mb->head->prev = p;
    mb->current = mb->head;
    mb->bottom = mb->head;

    return;
}


/* msgbuf_destroy
 * Frees the contents of a msgbuf and its structure. */
void msgbuf_destroy(msgbuf_t *mb)
{
    msgbufline_t *p;

    p = mb->head;
    while(p->next != mb->head) {
	string_delete(&p->line);

	p = p->next;
	d_memory_delete(p->prev);
    }

    d_memory_delete(p->next);
    d_memory_delete(p);

    return;
}

/* EOF msgbuf.c */
