/* 
 * netcommon.c
 * Created: Thu Jul 19 18:01:48 2001 by tek@wiw.org
 * Revised: Fri Jul 20 05:55:45 2001 by tek@wiw.org
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

#include <lua.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <limits.h>
#include <db.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "fobwart.h"
#include "fobclient.h"

#define READBYTE(d) { i = read(socket, &(d), 1); \
                      if(i == -1) goto error; \
                      if(i == 0) return failure; }

#define READWORD(d, t) { i = read(socket, &(t), 2); \
                         if(i == -1) goto error; \
                         if(i == 0) return failure; \
                         (d) = ntohs((t)); }

#define READDWORD(d, t) { i = read(socket, &(t), 4); \
                          if(i == -1) goto error; \
                          if(i == 0) return failure; \
                          (d) = ntohl((t)); }

#define WRITEBYTE(d) { i = write(socket, &(d), 1); \
                       if(i == -1) goto error; \
                       if(i == 0) return failure; }

#define WRITEWORD(d, t) { (t) = htons((d)); \
                          i = write(socket, &(t), 2); \
                          if(i == -1) goto error; \
                          if(i == 0) return failure; }


#define WRITEDWORD(d, t) { (t) = htonl((d)); \
                           i = write(socket, &(t), 4); \
                           if(i == -1) goto error; \
                           if(i == 0) return failure; }


bool readpack(int socket, packet_t *p)
{
    int i;
    byte bytebuf;
    word wordbuf;
    dword dwordbuf;

    i = read(socket, &p->type, 1);
    if(i == 0) return failure;
    if(i == -1) goto error;

    switch(p->type) {
    case PACK_YEAWHAW:
    case PACK_IRECKON:
        READBYTE(p->body.handshake);
        break;

    case PACK_EVENT:
        READWORD(p->body.event.subject, wordbuf);
        READBYTE(p->body.event.verb);
        READDWORD(p->body.event.auxlen, dwordbuf);
        if(p->body.event.auxlen > 0) {
            p->body.event.auxdata = d_memory_new(p->body.event.auxlen);
            if(p->body.event.auxdata == NULL)
                return failure;
            i = read(socket, p->body.event.auxdata, p->body.event.auxlen);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.event.auxdata = NULL;
        break;

    case PACK_LOGIN:
        READDWORD(p->body.login.namelen, dwordbuf);
        if(p->body.login.namelen > 0) {
            p->body.login.name = d_memory_new(p->body.login.namelen);
            if(p->body.login.name == NULL)
                return failure;
            i = read(socket, p->body.login.name, p->body.login.namelen);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.login.name = NULL;

        READDWORD(p->body.login.pwlen, dwordbuf);
        if(p->body.login.pwlen > 0) {
            p->body.login.password = d_memory_new(p->body.login.pwlen);
            if(p->body.login.password == NULL)
                return failure;
            i = read(socket, p->body.login.password, p->body.login.pwlen);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.login.password = NULL;
        break;

    case PACK_AYUP:
    case PACK_GETOBJECT:
    case PACK_GETROOM:
        READWORD(p->body.handle, wordbuf);
        break;

    case PACK_FRAME:
        break;

    case PACK_OBJECT:
        READDWORD(dwordbuf, dwordbuf);
        if(dwordbuf > 0) {
            p->body.object.name = d_memory_new(dwordbuf);
            if(p->body.object.name == NULL)
                return failure;
            i = read(socket, p->body.object.name, dwordbuf);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.object.name = NULL;

        READWORD(p->body.object.handle, wordbuf);
        READWORD(p->body.object.location, wordbuf);

        /* physics */

        READWORD(p->body.object.x, wordbuf);
        READWORD(p->body.object.y, wordbuf);
        READWORD(p->body.object.ax, wordbuf);
        READWORD(p->body.object.ay, wordbuf);
        READWORD(p->body.object.vx, wordbuf);
        READWORD(p->body.object.vy, wordbuf);

        READBYTE(bytebuf);
        p->body.object.onground = (bytebuf&1) ? true : false;
        p->body.object.facing = (bytebuf&2) ? left : right;

        /* statistics */
        READWORD(p->body.object.hp, wordbuf);
        READWORD(p->body.object.maxhp, wordbuf);

        /* graphics */
        READDWORD(dwordbuf, dwordbuf);
        if(dwordbuf > 0) {
            p->body.object.spname = d_memory_new(dwordbuf);
            if(p->body.object.spname == NULL)
                return failure;
            i = read(socket, p->body.object.spname, dwordbuf);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.object.spname = NULL;
        p->body.object.sprite = NULL;

        /* scripts */
        break;

    case PACK_ROOM:
        READDWORD(dwordbuf, dwordbuf);
        if(dwordbuf > 0) {
            p->body.room.name = d_memory_new(dwordbuf);
            if(p->body.room.name == NULL)
                return failure;
            i = read(socket, p->body.room.name, dwordbuf);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.room.name = NULL;

        READWORD(p->body.room.handle, wordbuf);

        /* physics */
        READWORD(p->body.room.gravity, wordbuf);
        READBYTE(bytebuf);
        p->body.room.islit = (bytebuf&1) ? true:false;

        /* tilemaps */
        READDWORD(dwordbuf, dwordbuf);
        if(dwordbuf > 0) {
            p->body.room.mapname = d_memory_new(dwordbuf);
            if(p->body.room.mapname == NULL)
                return failure;
            i = read(socket, p->body.room.mapname, dwordbuf);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.room.mapname = NULL;
        p->body.room.map = NULL;

        READDWORD(dwordbuf, dwordbuf);
        if(dwordbuf > 0) {
            p->body.room.bgname = d_memory_new(dwordbuf);
            if(p->body.room.bgname == NULL)
                return failure;
            i = read(socket, p->body.room.bgname, dwordbuf);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.room.bgname = NULL;
        p->body.room.bg = NULL;

        /* contents */
        p->body.room.contents = NULL;

        /* scripts */
        break;

    default:
        d_error_debug(__FUNCTION__": default. (%d)\n", p->type);
        goto error;
    }

    return success;
error:
    d_error_debug(__FUNCTION__": %s\n", strerror(errno));
    return failure;    
}

bool writepack(int socket, packet_t p)
{
    int i;
    byte bytebuf;
    word wordbuf;
    dword dwordbuf;

    i = write(socket, &p.type, 1);
    if(i == -1) goto error;

    switch(p.type) {
    case PACK_YEAWHAW:
    case PACK_IRECKON:
        WRITEBYTE(p.body.handshake);
        break;

    case PACK_EVENT:
        WRITEWORD(p.body.event.subject, wordbuf);
        WRITEBYTE(p.body.event.verb);
        WRITEDWORD(p.body.event.auxlen, dwordbuf);
        if(p.body.event.auxlen > 0) {
            i = write(socket, p.body.event.auxdata, p.body.event.auxlen);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }
        break;

    case PACK_LOGIN:
        WRITEDWORD(p.body.login.namelen, dwordbuf);
        if(p.body.login.namelen > 0) {
            i = write(socket, p.body.login.name, p.body.login.namelen);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }

        WRITEDWORD(p.body.login.pwlen, dwordbuf);
        if(p.body.login.pwlen > 0) {
            i = write(socket, p.body.login.password, p.body.login.pwlen);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }
        break;

    case PACK_AYUP:
    case PACK_GETOBJECT:
    case PACK_GETROOM:
        WRITEWORD(p.body.handle, wordbuf);
        break;

    case PACK_FRAME:
        break;

    case PACK_OBJECT:
        WRITEDWORD(strlen(p.body.object.name)+1, dwordbuf);
        if(strlen(p.body.object.name) > 0) {
            i = write(socket, p.body.object.name,
                      strlen(p.body.object.name)+1);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }

        WRITEWORD(p.body.object.handle, wordbuf);
        WRITEWORD(p.body.object.location, wordbuf);

        /* physics */

        WRITEWORD(p.body.object.x, wordbuf);
        WRITEWORD(p.body.object.y, wordbuf);
        WRITEWORD(p.body.object.ax, wordbuf);
        WRITEWORD(p.body.object.ay, wordbuf);
        WRITEWORD(p.body.object.vx, wordbuf);
        WRITEWORD(p.body.object.vy, wordbuf);

        bytebuf = 0;
        bytebuf |= (p.body.object.onground == true) ? 1:0;
        bytebuf |= (p.body.object.facing == left) ? 2:0;
        WRITEBYTE(bytebuf);

        /* statistics */
        WRITEWORD(p.body.object.hp, wordbuf);
        WRITEWORD(p.body.object.maxhp, wordbuf);

        /* graphics */
        WRITEDWORD(strlen(p.body.object.spname)+1, dwordbuf);
        if(strlen(p.body.object.spname) > 0) {
            i = write(socket, p.body.object.spname,
                      strlen(p.body.object.spname)+1);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }

        /* scripts */
        break;

    case PACK_ROOM:
        WRITEDWORD(strlen(p.body.room.name)+1, dwordbuf);
        if(strlen(p.body.room.name) > 0) {
            i = write(socket, p.body.room.name,
                      strlen(p.body.room.name)+1);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }

        WRITEWORD(p.body.room.handle, wordbuf);

        /* physics */
        WRITEWORD(p.body.room.gravity, wordbuf);
        bytebuf = 0;
        bytebuf |= (p.body.room.islit == true) ? 1:0;
        WRITEBYTE(bytebuf);

        /* tilemaps */
        WRITEDWORD(strlen(p.body.room.mapname)+1, dwordbuf);
        if(strlen(p.body.room.mapname) > 0) {
            i = write(socket, p.body.room.mapname,
                      strlen(p.body.room.mapname)+1);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }

        WRITEDWORD(strlen(p.body.room.bgname)+1, dwordbuf);
        if(strlen(p.body.room.bgname) > 0) {
            i = write(socket, p.body.room.bgname,
                      strlen(p.body.room.bgname)+1);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }

        /* scripts */
        break;

    default:
        goto error;
    }

    return success;
error:
    d_error_debug(__FUNCTION__": %s\n", strerror(errno));
    return failure;    
}

/* EOF netcommon.c */
