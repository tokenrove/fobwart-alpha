/* 
 * fobserv.c
 * Created: Wed Jul 18 03:15:09 2001 by tek@wiw.org
 * Revised: Thu Jul 19 16:28:34 2001 by tek@wiw.org
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

#include <lua.h>

#include "fobwart.h"
#include "fobnet.h"

#define PORT 6400
#define PROGNAME "fobserv"

typedef struct client_s {
    word handle;
    int socket;
    int state;
} client_t;

typedef struct serverdata_s {
    d_set_t *rooms;
    d_set_t *objs;
} serverdata_t;

int initlisten(int *sock);
int handleclient(client_t *cli, eventstack_t *evsk);
int handshake(int clisock);
void sendevents(d_set_t *clients, eventstack_t *evsk);

int main(void)
{
    int servsock, clisock;
    d_set_t *clients;
    client_t *cli;
    int nfds, nframed, i;
    fd_set readfds, readfdbak;
    dword key;
    struct timeval timeout;
    bool status;
    eventstack_t evsk;

    if(initlisten(&servsock) == -1) {
        fprintf(stderr, "%s: failed to initialize listening socket.\n",
                PROGNAME);
        exit(EXIT_FAILURE);
    }

    clients = d_set_new(0);
    if(clients == NULL) {
        fprintf(stderr, "%s: unable to allocate client set.\n", PROGNAME);
        exit(EXIT_FAILURE);
    }

    evsk_new(&evsk);
    nframed = 0;

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(servsock, &readfds);
        nfds = servsock;
        d_set_resetiteration(clients);
        while(key = d_set_nextkey(clients), key != D_SET_INVALIDKEY) {
            d_set_fetch(clients, key, (void **)&cli);
            if(cli->socket > nfds) nfds = cli->socket;
            FD_SET(cli->socket, &readfds);
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = 16000;

        d_memory_copy(&readfdbak, &readfds, sizeof(readfds));
        while(select(nfds+1, &readfds, NULL, NULL, &timeout) == 0) {
            timeout.tv_sec = 0;
            timeout.tv_usec = 16000;
            d_memory_copy(&readfds, &readfdbak, sizeof(readfdbak));
        }

        if(FD_ISSET(servsock, &readfds)) {
            clisock = accept(servsock, NULL, NULL);
            if(clisock == -1) {
                fprintf(stderr, "%s: accept: %s\n", PROGNAME, strerror(errno));
            } else if(handshake(clisock) == 0) {
                cli = d_memory_new(sizeof(client_t));
                cli->handle = d_set_getunusedkey(clients);
                cli->socket = clisock;
                cli->state = 0;
                status = d_set_add(clients, cli->handle, (void *)cli);
                if(status != success)
                    fprintf(stderr, "%s: d_set_add failed\n", PROGNAME);
            }
        }

        d_set_resetiteration(clients);
        while(key = d_set_nextkey(clients), key != D_SET_INVALIDKEY) {
            d_set_fetch(clients, key, (void **)&cli);
            if(FD_ISSET(cli->socket, &readfds)) {
                i = handleclient(cli, &evsk);
                if(i == -1) {
                    close(cli->socket);
                    d_set_remove(clients, key);
                } else if(i == 1) {
                    nframed++;
                }
            }
        }

        if(nframed == d_set_nelements(clients)) {
            sendevents(clients, &evsk);
            nframed = 0;
        }
    }

    evsk_delete(&evsk);
    d_set_delete(clients);

    exit(EXIT_SUCCESS);
}

void sendevents(d_set_t *clients, eventstack_t *evsk)
{
    event_t ev;
    client_t *cli;
    dword key;
    packet_t p;

    while(evsk_top(evsk, &ev)) {
        d_set_resetiteration(clients);
        while(key = d_set_nextkey(clients), key != D_SET_INVALIDKEY) {
            d_set_fetch(clients, key, (void **)&cli);
            if(cli->handle != ev.subject) {
                p.type = PACK_EVENT;
                p.body.event = ev;
                writepack(cli->socket, p);
            }
        }
        evsk_pop(evsk);
    }

    d_set_resetiteration(clients);
    while(key = d_set_nextkey(clients), key != D_SET_INVALIDKEY) {
        d_set_fetch(clients, key, (void **)&cli);
        p.type = PACK_FRAME;
        writepack(cli->socket, p);
        cli->state = 2;
    }
    return;
}

int handshake(int clisock)
{
    packet_t p;

    p.type = PACK_YEAWHAW;
    p.body.handshake = 42;
    writepack(clisock, p);

    if(readpack(clisock, &p) == failure)
        return -1;
    if(p.type != PACK_IRECKON) {
        fprintf(stderr, "%s: bad handshake. moving on.", PROGNAME);
        close(clisock);
        return -1;
    }
    printf("handshake: %d\n", p.body.handshake);
    return 0;
}

int initlisten(int *sock)
{
    struct sockaddr_in addr;

    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if(*sock == -1)
        return -1;

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if(bind(*sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
        return -1;

    listen(*sock, 5);
    return 0;
}

int handleclient(client_t *cli, eventstack_t *evsk)
{
    packet_t p;

    if(readpack(cli->socket, &p) == failure) return -1;

    switch(cli->state) {
    case 0: /* expect login, all other packets rejected */
        if(p.type == PACK_LOGIN) {
            fprintf(stderr, "Login from %d (%s/%s)\n", cli->handle,
                    p.body.login.name, p.body.login.password);
            p.type = PACK_AYUP;
            p.body.handle = cli->handle;
            if(writepack(cli->socket, p) == failure) return -1;
            cli->state++;

        } else {
            fprintf(stderr, "%d tried to skip login phase.\n",
                    cli->handle);
            return -1;
        }
        break;

    case 1: /* expect sync */
    case 2: /* events */
        if(p.type == PACK_EVENT) {
            if(p.body.event.subject != cli->handle) {
                fprintf(stderr, "Got event from %d [calls itself %d]: %d + ``%s''\n",
                        cli->handle, p.body.event.subject, p.body.event.verb,
                        (p.body.event.auxlen)?p.body.event.auxdata:"");
            }
            evsk_push(evsk, p.body.event);
        } else if(p.type == PACK_FRAME) {
            cli->state = 3;
            return 1; /* return that we're framed */
        }
        break;

    case 3: /* framed */
        fprintf(stderr, __FUNCTION__": %d broken! should be framed!\n",
                cli->handle);
        return -1;
    }

    return 0;
}

/* EOF fobserv.c */
