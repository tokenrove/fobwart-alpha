/* 
 * fobserv.h
 * Created: Thu Jul 19 19:20:03 2001 by tek@wiw.org
 * Revised: Thu Jul 19 19:23:08 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

typedef struct loginrec_s {
    char *password;
    objhandle_t object;
} loginrec_t;

typedef struct client_s {
    objhandle_t handle;
    int socket;
    int state;
} client_t;

typedef struct serverdata_s {
    d_set_t *rooms;
    d_set_t *objs;
    d_set_t *clients;
    eventstack_t evsk;
    int socket;
} serverdata_t;

/* EOF fobserv.h */
