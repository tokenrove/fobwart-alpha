/* 
 * network.c
 * Created: Wed Jul 18 01:29:32 2001 by tek@wiw.org
 * Revised: Thu Jul 19 16:49:40 2001 by tek@wiw.org
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

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "fobwart.h"
#include "fobnet.h"

bool initnet(gamedata_t *gd, char *servname, int port);
void closenet(gamedata_t *gd);
bool login(gamedata_t *gd, char *uname, char *password);
void syncevents(gamedata_t *gd);

bool readpack(int socket, packet_t *p);
bool writepack(int socket, packet_t p);

bool initnet(gamedata_t *gd, char *servname, int port)
{
    struct hostent *hostent;
    struct sockaddr_in sockaddr;
    packet_t p;
    bool status;

    gd->socket = socket(AF_INET, SOCK_STREAM, 0);
    if(gd->socket == -1) {
        d_error_debug(__FUNCTION__": %s\n", strerror(errno));
        return failure;
    }

    sockaddr.sin_family = AF_INET;
    hostent = gethostbyname(servname);
    if(hostent == NULL) {
        d_error_debug(__FUNCTION__": %s\n", strerror(errno));
        return failure;
    }

    memcpy(&sockaddr.sin_addr, hostent->h_addr_list[0], hostent->h_length);
    sockaddr.sin_port = htons(port);

    if(connect(gd->socket, (const struct sockaddr *)&sockaddr,
               sizeof(sockaddr)) == -1) {
        d_error_debug(__FUNCTION__": %s\n", strerror(errno));
        return failure;
    }

    /* Handshake */
    status = readpack(gd->socket, &p);
    if(status == failure || p.type != PACK_YEAWHAW)
        return failure;
    d_error_debug("PACK_YEAWHAW: %d\n", p.body.handshake);
    p.type = PACK_IRECKON;
    p.body.handshake = 42;
    status = writepack(gd->socket, p);
    if(status == failure)
        return failure;

    return success;
}

void closenet(gamedata_t *gd)
{
    close(gd->socket);
    gd->socket = -1;
    return;
}

bool login(gamedata_t *gd, char *uname, char *password)
{
    packet_t p;

    p.type = PACK_LOGIN;
    p.body.login.name = uname;
    p.body.login.namelen = strlen(uname)+1;
    p.body.login.password = password;
    p.body.login.pwlen = strlen(password)+1;
    writepack(gd->socket, p);
    if(readpack(gd->socket, &p) != success)
        return failure;
    if(p.type != PACK_AYUP) {
        d_error_debug(__FUNCTION__": Login failed!\n");
        return failure;
    }
    gd->localobj = p.body.handle;
    return success;
}

void syncevents(gamedata_t *gd)
{
    int i;
    packet_t p;

    for(i = 0; i < gd->evsk.top; i++) {
        p.type = PACK_EVENT;
        p.body.event = gd->evsk.events[i];
        writepack(gd->socket, p);
    }
    p.type = PACK_FRAME;
    i = writepack(gd->socket, p);
    if(i == -1)
        d_error_debug(__FUNCTION__": write frame failed.\n");
    while(i = readpack(gd->socket, &p), i != -1 && p.type != PACK_FRAME) {
        if(p.type != PACK_EVENT)
            d_error_debug(__FUNCTION__": read event failed.\n");
        evsk_push(&gd->evsk, p.body.event);
    }
    if(i == -1)
        d_error_debug(__FUNCTION__": read frame failed.\n");
    return;
}

bool readpack(int socket, packet_t *p)
{
    int i;
    word wordbuf;
    dword dwordbuf;

    i = read(socket, &p->type, 1);
    if(i == 0) return failure;
    if(i == -1) goto error;

    switch(p->type) {
    case PACK_YEAWHAW:
    case PACK_IRECKON:
        i = read(socket, &p->body.handshake, 1);
        if(i == -1) goto error;
        if(i == 0) return failure;
        break;

    case PACK_EVENT:
        i = read(socket, &wordbuf, 2);
        if(i == -1) goto error;
        if(i == 0) return failure;
        p->body.event.subject = ntohs(wordbuf);

        i = read(socket, &p->body.event.verb, 1);
        if(i == -1) goto error;
        if(i == 0) return failure;

        i = read(socket, &dwordbuf, 4);
        if(i == -1) goto error;
        if(i == 0) return failure;
        p->body.event.auxlen = ntohl(dwordbuf);

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
        i = read(socket, &dwordbuf, 4);
        if(i == -1) goto error;
        if(i == 0) return failure;
        p->body.login.namelen = ntohl(dwordbuf);

        if(p->body.login.namelen > 0) {
            p->body.login.name = d_memory_new(p->body.login.namelen);
            if(p->body.login.name == NULL)
                return failure;
            i = read(socket, p->body.login.name, p->body.login.namelen);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.login.name = NULL;

        i = read(socket, &dwordbuf, 4);
        if(i == -1) goto error;
        if(i == 0) return failure;
        p->body.login.pwlen = ntohl(dwordbuf);

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
        i = read(socket, &wordbuf, 2);
        if(i == -1) goto error;
        if(i == 0) return failure;
        p->body.handle = ntohs(wordbuf);
        break;

    case PACK_FRAME:
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
    word wordbuf;
    dword dwordbuf;

    i = write(socket, &p.type, 1);
    if(i == -1) goto error;

    switch(p.type) {
    case PACK_YEAWHAW:
    case PACK_IRECKON:
        i = write(socket, &p.body.handshake, 1);
        if(i == -1) goto error;
        if(i == 0) d_error_debug(__FUNCTION__": blank write\n");
        break;

    case PACK_EVENT:
        wordbuf = htons(p.body.event.subject);
        i = write(socket, &wordbuf, 2);
        if(i == -1) goto error;
        if(i == 0) return failure;

        i = write(socket, &p.body.event.verb, 1);
        if(i == -1) goto error;
        if(i == 0) return failure;

        dwordbuf = htonl(p.body.event.auxlen);
        i = write(socket, &dwordbuf, 4);
        if(i == -1) goto error;
        if(i == 0) return failure;

        if(p.body.event.auxlen > 0) {
            i = write(socket, p.body.event.auxdata, p.body.event.auxlen);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }
        break;

    case PACK_LOGIN:
        dwordbuf = htonl(p.body.login.namelen);
        i = write(socket, &dwordbuf, 4);
        if(i == -1) goto error;
        if(i == 0) return failure;

        if(p.body.login.namelen > 0) {
            i = write(socket, p.body.login.name, p.body.login.namelen);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }

        dwordbuf = htonl(p.body.login.pwlen);
        i = write(socket, &dwordbuf, 4);
        if(i == -1) goto error;
        if(i == 0) return failure;

        if(p.body.login.pwlen > 0) {
            i = write(socket, p.body.login.password, p.body.login.pwlen);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }
        break;

    case PACK_AYUP:
        wordbuf = htons(p.body.handle);
        i = write(socket, &wordbuf, 2);
        if(i == -1) goto error;
        if(i == 0) return failure;
        break;

    case PACK_FRAME:
        break;

    default:
        goto error;
    }

    return success;
error:
    d_error_debug(__FUNCTION__": %s\n", strerror(errno));
    return failure;    
}

/* EOF network.c */
