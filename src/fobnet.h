/* 
 * fobnet.h
 * Created: Wed Jul 18 03:53:16 2001 by tek@wiw.org
 * Revised: Wed Jul 18 04:12:35 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

#ifndef FOBNET_H
#define FOBNET_H

enum { PACK_YEAWHAW = 0, PACK_IRECKON, PACK_AYUP,
       PACK_LOGIN, PACK_SYNC, PACK_EVENT };

typedef struct packet_s {
    byte type;
    union {
        byte handshake;
        event_t event;
    } body;
} packet_t;

extern bool readpack(int socket, packet_t *p);
extern bool writepack(int socket, packet_t p);

#endif

/* EOF fobnet.h */
