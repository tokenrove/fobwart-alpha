/*
 * servnet.c ($Id$)
 * Julian Squires <tek@wiw.org> / 2001
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
#include <dentata/pcx.h>
#include <dentata/random.h>
#include <dentata/util.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <db.h>
#include <fcntl.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#include <lua.h>

#include "fobwart.h"
#include "fobnet.h"
#include "fobdb.h"
#include "fobserv.h"

#include "net.h"


void net_servselect(nethandle_t *servnh_, bool *servisactive, d_set_t *clients,
                    dword *activeclients);
bool net_accept(nethandle_t *servnh_, nethandle_t **clinh_);
static bool handshake(nethandle_t *nh_);


void net_servselect(nethandle_t *servnh_, bool *servisactive, d_set_t *clients,
                    dword *activeclients)
{
    nh_t *nh;
    client_t *cli;
    int nfds, i;
    fd_set readfds;
    dword key;
    struct timeval timeout;
    d_iterator_t it;

    /* set readfds */
    FD_ZERO(&readfds);
    nh = servnh_;
    FD_SET(nh->socket, &readfds);
    nfds = nh->socket;

    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, clients), key != D_SET_INVALIDKEY) {
        d_set_fetch(clients, key, (void **)&cli);
        nh = (nh_t *)cli->nh;
        if(nh->socket > nfds) nfds = nh->socket;
        FD_SET(nh->socket, &readfds);
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 16000;
    select(nfds+1, &readfds, NULL, NULL, &timeout);

    nh = servnh_;
    if(FD_ISSET(nh->socket, &readfds))
        *servisactive = true;
    else
        *servisactive = false;

    i = 0;
    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, clients), key != D_SET_INVALIDKEY) {
        d_set_fetch(clients, key, (void **)&cli);
        nh = (nh_t *)cli->nh;
        if(FD_ISSET(nh->socket, &readfds))
            activeclients[i++] = key;
    }
    activeclients[i] = D_SET_INVALIDKEY;
    return;
}


bool net_accept(nethandle_t *servnh_, nethandle_t **clinh_)
{
    nh_t *servnh, *clinh;

    servnh = servnh_;
    clinh = d_memory_new(sizeof(nh_t));
    if(clinh == NULL) {
        *clinh_ = NULL;
        return failure;
    }

    /* accept a new connection */
    clinh->socket = accept(servnh->socket, NULL, NULL);
    if(clinh->socket == -1) {
        d_error_debug(__FUNCTION__": accept: %s\n", strerror(errno));
        return failure;
    }

    *clinh_ = clinh;

    if(handshake(*clinh_) != 0)
        return failure;

    return success;
}


static bool handshake(nethandle_t *nh_)
{
    packet_t p;

    p.type = PACK_YEAWHAW;
    p.body.handshake = 42;
    net_writepack(nh_, p);

    if(net_readpack(nh_, &p) == failure)
        return failure;
    if(p.type != PACK_IRECKON) {
        d_error_debug(__FUNCTION__": bad handshake. moving on.");
        return failure;
    }
    return success;
}

/* EOF servnet.c */
