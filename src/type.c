/*
 * type.c ($Id$)
 * Julian Squires <tek@wiw.org> / 2001
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
#include <dentata/event.h>
#include <dentata/memory.h>

#include <lua.h>

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"


void insertchar(typebuf_t *type, int i, bool shift);
int handletextinput(typebuf_t *type);


#define TYPEALLOCINC 16

void insertchar(typebuf_t *type, int i, bool shift)
{
    char xlate[2][128] = { {
        '?', '?', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', /* 0-11 */
        '-', '=', '?', '?', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', /* 12-23 */
        'o', 'p', '[', ']', '?', '?', 'a', 's', 'd', 'f', 'g', 'h', /* 24-35 */
        'j', 'k', 'l', ';','\'', '`', '?','\\', 'z', 'x', 'c', 'v', /* 36-47 */
        'b', 'n', 'm', ',', '.', '/', '?', '*', '?', ' ', /* 48-57 */
        '?', '?', '?', '?', '?', '?', '?', '?', '?',
        '?', '?', '?', '?', '?', '?', '?', '?', '?',
        '?', '?', '?', '?', '?', '?', '?', '?', '?',
        '?', '?', '?', '?', '?', '?', '?', '?', '?',
        '?', '?', '?', '?', '?', '?', '?', '?', '?',
        '?', '?', '?', '?', '?', '?', '?', '?', '?',
        '?', '?', '?', '?', '?', '?', '?', '?'
    }, {
        '?', '?', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', /* 0-11 */
        '_', '+', '?', '?', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', /* 12-23 */
        'O', 'P', '{', '}', '?', '?', 'A', 'S', 'D', 'F', 'G', 'H', /* 24-35 */
        'J', 'K', 'L', ':', '"', '~', '?', '|', 'Z', 'X', 'C', 'V', /* 36-47 */
        'B', 'N', 'M', '<', '>', '?', '?', '*', '?', ' ', /* 48-57 */
        '?', '?', '?', '?', '?', '?', '?', '?', '?',
        '?', '?', '?', '?', '?', '?', '?', '?', '?',
        '?', '?', '?', '?', '?', '?', '?', '?', '?',
        '?', '?', '?', '?', '?', '?', '?', '?', '?',
        '?', '?', '?', '?', '?', '?', '?', '?', '?',
        '?', '?', '?', '?', '?', '?', '?', '?', '?',
        '?', '?', '?', '?', '?', '?', '?', '?'
    } };

    if(type->pos+1 >= type->nalloc) {
        type->nalloc += TYPEALLOCINC;
        type->buf = d_memory_resize(type->buf, type->nalloc);
        if(type->buf == NULL)
            d_error_fatal(__FUNCTION__": Failed to reallocate type buffer.\n");
        d_memory_set(type->buf+type->pos, 0, type->nalloc-type->pos);
    }
    type->buf[type->pos++] = xlate[shift][i];
}


int handletextinput(typebuf_t *type)
{
    int i;
    int keymap[48] = { D_KBD_GRAVE, D_KBD_1, D_KBD_2, D_KBD_3, D_KBD_4,
                       D_KBD_5, D_KBD_6, D_KBD_7, D_KBD_8, D_KBD_9,
                       D_KBD_0, D_KBD_MINUS, D_KBD_EQUALS, D_KBD_Q,
                       D_KBD_W, D_KBD_E, D_KBD_R, D_KBD_T, D_KBD_Y,
                       D_KBD_U, D_KBD_I, D_KBD_O, D_KBD_P, D_KBD_LBRACK,
                       D_KBD_RBRACK, D_KBD_BACKSLASH, D_KBD_A, D_KBD_S,
                       D_KBD_D, D_KBD_F, D_KBD_G, D_KBD_H, D_KBD_J,
                       D_KBD_K, D_KBD_L, D_KBD_SEMICOLON, D_KBD_APOSTROPHE,
                       D_KBD_Z, D_KBD_X, D_KBD_C, D_KBD_V, D_KBD_B,
                       D_KBD_N, D_KBD_M, D_KBD_COMMA, D_KBD_PERIOD,
                       D_KBD_SLASH, D_KBD_SPACE };

    if(d_event_ispressed(EV_ENTER) && !isbounce(EV_ENTER)) {
        return 1;
    }

    for(i = EV_ALPHABEGIN; i < EV_ALPHAEND; i++)
        if(d_event_ispressed(i) && !isbounce(i)) {
            insertchar(type, keymap[i-EV_ALPHABEGIN],
                       d_event_ispressed(EV_SHIFT));
        }
    if(d_event_ispressed(EV_BACKSPACE) && !isbounce(EV_BACKSPACE))
        if(type->pos > 0)
            type->buf[type->pos--] = 0;

    return 0;
}

/* EOF type.c */
