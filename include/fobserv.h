/* 
 * fobserv.h
 * Created: Thu Jul 19 19:20:03 2001 by tek@wiw.org
 * Revised: Fri Jul 27 00:23:23 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

typedef struct client_s {
    objhandle_t handle;
    nethandle_t *nh;
    int state;
} client_t;

typedef struct serverdata_s {
    worldstate_t ws;
    d_set_t *clients;
    nethandle_t *nh;
    dbhandle_t *logindb, *objectdb, *roomdb;
    reslist_t reslist;
} serverdata_t;


/* servdata.c */
extern bool loadservdata(serverdata_t *sd);
extern bool getobject(serverdata_t *sd, objhandle_t key);
extern bool loadfile(char *name, byte **data, dword *len);

/* EOF fobserv.h */
