/* 
 * network.c ($Id$)
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 *
 * Low-level and common networking routines.
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
#include <dentata/random.h>
#include <dentata/util.h>

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
#include "fobnet.h"

#include "net.h"


void net_close(nethandle_t *nh_);
bool net_login(nethandle_t *nh, char *uname, char *password,
               objhandle_t *localobj);
bool net_newclient(nethandle_t **nh_, char *servname, int port);
bool net_newserver(nethandle_t **nh_, int port);
bool net_canread(nethandle_t *nh_);

bool net_readpack(nethandle_t *nh_, packet_t *p);
bool net_writepack(nethandle_t *nh_, packet_t p);

static int readbuffer(int socket, char *data, dword len);
static int writebuffer(int socket, char *data, dword len);


bool net_newclient(nethandle_t **nh_, char *servname, int port)
{
    struct hostent *hostent;
    struct sockaddr_in sockaddr;
    packet_t p;
    bool status;
    nh_t *nh;

    nh = d_memory_new(sizeof(nh_t));
    if(nh == NULL) {
        *nh_ = NULL;
        return failure;
    }

    *nh_ = nh;

    nh->socket = socket(AF_INET, SOCK_STREAM, 0);
    if(nh->socket == -1) {
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

    if(connect(nh->socket, (const struct sockaddr *)&sockaddr,
               sizeof(sockaddr)) == -1) {
        d_error_debug(__FUNCTION__": %s\n", strerror(errno));
        return failure;
    }

    /* Handshake */
    status = net_readpack(*nh_, &p);
    if(status == failure || p.type != PACK_YEAWHAW)
        return failure;
    p.type = PACK_IRECKON;
    p.body.handshake = 42;
    status = net_writepack(*nh_, p);
    if(status == failure)
        return failure;

    return success;
}


bool net_newserver(nethandle_t **nh_, int port)
{
    struct sockaddr_in addr;
    nh_t *nh;

    nh = d_memory_new(sizeof(nh_t));
    if(nh == NULL) {
        *nh_ = NULL;
        return failure;
    }

    nh->socket = socket(AF_INET, SOCK_STREAM, 0);
    if(nh->socket == -1)
        return failure;

    d_memory_set(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if(bind(nh->socket, (struct sockaddr *)&addr,
            sizeof(struct sockaddr_in)) == -1)
        return failure;

    listen(nh->socket, 5);
    *nh_ = nh;

    return success;
}


bool net_login(nethandle_t *nh, char *uname, char *password,
               objhandle_t *localobj)
{
    packet_t p;
    bool status;

    p.type = PACK_LOGIN;
    status = string_fromasciiz(&p.body.login.name, uname);
    status &= string_fromasciiz(&p.body.login.password, password);
    status &= net_writepack(nh, p);
    string_delete(&p.body.login.name);
    string_delete(&p.body.login.password);

    status &= net_readpack(nh, &p);
    if(status != success || p.type != PACK_AYUP)
        return failure;
    if(localobj != NULL)
        *localobj = p.body.handle;
    return success;
}


void net_close(nethandle_t *nh_)
{
    nh_t *nh = nh_;

    close(nh->socket);
    nh->socket = -1;
    d_memory_delete(nh);
    return;
}


/* net_canread
 * Check if there is data to be read from the given nethandle. */
bool net_canread(nethandle_t *nh_)
{
    nh_t *nh = nh_;
    fd_set readfds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_SET(nh->socket, &readfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    return (select(1, &readfds, NULL, NULL, &timeout) == 1) ? true : false;
}


#define READBYTE(d) { i = read(nh->socket, &(d), 1); \
                      if(i == -1) goto error; \
                      if(i == 0) return failure; }

#define READWORD(d) { i = read(nh->socket, &(d), 2); \
                      if(i == -1) goto error; \
                      if(i == 0) return failure; \
                      (d) = ntohs((d)); }

#define READDWORD(d) { i = read(nh->socket, &(d), 4); \
                       if(i == -1) goto error; \
                       if(i == 0) return failure; \
                       (d) = ntohl((d)); }

#define READSTRING(s) { READDWORD((s).len); \
                        if((s).len > 0) { \
                            (s).data = d_memory_new((s).len); \
                            if((s).data == NULL) return failure; \
                            i = readbuffer(nh->socket, (char *)(s).data, (s).len); \
                            if(i == -1) goto error; \
                            if(i == 0) return failure; \
                        } else \
                           (s).data = NULL; }

#define WRITEBYTE(d) { i = write(nh->socket, &(d), 1); \
                       if(i == -1) goto error; \
                       if(i == 0) return failure; }

#define WRITEWORD(d, t) { (t) = htons((d)); \
                          i = write(nh->socket, &(t), 2); \
                          if(i == -1) goto error; \
                          if(i == 0) return failure; }


#define WRITEDWORD(d, t) { (t) = htonl((d)); \
                           i = write(nh->socket, &(t), 4); \
                           if(i == -1) goto error; \
                           if(i == 0) return failure; }



bool net_readpack(nethandle_t *nh_, packet_t *p)
{
    int i, j;
    byte bytebuf;
    word wordbuf;
    dword dwordbuf, key;
    dword statelen;
    nh_t *nh = nh_;
    char *statebuf, *s;

    i = read(nh->socket, &p->type, 1);
    if(i == 0) return failure;
    if(i == -1) goto error;

    switch(p->type) {
    case PACK_YEAWHAW:
    case PACK_IRECKON:
        READBYTE(p->body.handshake);
        break;

    case PACK_EVENT:
        READWORD(p->body.event.subject);
        READBYTE(p->body.event.verb);
        READDWORD(p->body.event.auxlen);
        if(p->body.event.auxlen > 0) {
            p->body.event.auxdata = d_memory_new(p->body.event.auxlen);
            if(p->body.event.auxdata == NULL)
                return failure;
            i = readbuffer(nh->socket, p->body.event.auxdata,
			   p->body.event.auxlen);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.event.auxdata = NULL;
        break;

    case PACK_LOGIN:
	READSTRING(p->body.login.name);
	READSTRING(p->body.login.password);
        break;

    case PACK_AYUP:
    case PACK_GETOBJECT:
    case PACK_GETROOM:
        READWORD(p->body.handle);
        break;

    case PACK_FRAME:
    case PACK_GETRESLIST:
        break;

    case PACK_GETFILE:
	READSTRING(p->body.string);
	break;

    case PACK_FILE:
	READSTRING(p->body.file.name);
	READDWORD(p->body.file.length);
	READDWORD(p->body.file.checksum);
	p->body.file.data = d_memory_new(p->body.file.length);
	i = readbuffer(nh->socket, (char *)p->body.file.data,
		       p->body.file.length);
	if(i == -1) goto error;
	if(i == 0) return failure;
	break;

    case PACK_RESLIST:
	READWORD(p->body.reslist.length);
	p->body.reslist.name = d_memory_new(p->body.reslist.length*
					    sizeof(string_t));
	p->body.reslist.checksum = d_memory_new(p->body.reslist.length*
						sizeof(dword));

	for(j = 0; j < p->body.reslist.length; j++) {
	    READDWORD(p->body.reslist.name[j].len);

	    p->body.reslist.name[j].data = d_memory_new(p->body.reslist.name[j].len);
	    i = readbuffer(nh->socket, (char *)p->body.reslist.name[j].data,
			   p->body.reslist.name[j].len);
	    if(i == -1) goto error;
	    if(i == 0) return failure;

	    READDWORD(p->body.reslist.checksum[j]);
	}
	break;

    case PACK_OBJECT:
        READDWORD(dwordbuf);
        if(dwordbuf > 0) {
            p->body.object.name = d_memory_new(dwordbuf);
            if(p->body.object.name == NULL)
                return failure;
            i = readbuffer(nh->socket, p->body.object.name, dwordbuf);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.object.name = NULL;

        READWORD(p->body.object.handle);
        READWORD(p->body.object.location);

        /* physics */
        READWORD(p->body.object.x);
        READWORD(p->body.object.y);
        READWORD(p->body.object.ax);
        READWORD(p->body.object.ay);
        READWORD(p->body.object.vx);
        READWORD(p->body.object.vy);

        READBYTE(bytebuf);
        p->body.object.onground = (bytebuf&1) ? true : false;

        /* graphics */
        READDWORD(dwordbuf);
        if(dwordbuf > 0) {
            p->body.object.spname = d_memory_new(dwordbuf);
            if(p->body.object.spname == NULL)
                return failure;
            i = readbuffer(nh->socket, p->body.object.spname, dwordbuf);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.object.spname = NULL;
        p->body.object.sprite = NULL;

        /* scripts */
        READDWORD(statelen);
        if(statelen > 0) {
            statebuf = d_memory_new(statelen);
            if(statebuf == NULL) return failure;

            i = readbuffer(nh->socket, statebuf, statelen);
            if(i == -1) goto error;
            if(i == 0) return failure;

        } else
	    statebuf = NULL;

	deskelobject(&p->body.object);

	if(statelen > 0) {
	    meltstate(p->body.object.luastate, statebuf);
	    d_memory_delete(statebuf);
	}

        break;

    case PACK_ROOM:
        READDWORD(dwordbuf);
        if(dwordbuf > 0) {
            p->body.room.name = d_memory_new(dwordbuf);
            if(p->body.room.name == NULL)
                return failure;
            i = readbuffer(nh->socket, p->body.room.name, dwordbuf);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.room.name = NULL;

        READDWORD(dwordbuf);
        if(dwordbuf > 0) {
            p->body.room.owner = d_memory_new(dwordbuf);
            if(p->body.room.owner == NULL)
                return failure;
            i = readbuffer(nh->socket, p->body.room.owner, dwordbuf);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.room.owner = NULL;

        READWORD(p->body.room.handle);

        /* physics */
        READWORD(p->body.room.gravity);
        READBYTE(bytebuf);
        p->body.room.islit = (bytebuf&1) ? true:false;

        /* tilemaps */
	p->body.room.map = d_memory_new(sizeof(d_tilemap_t));
	d_memory_set(p->body.room.map, 0, sizeof(d_tilemap_t));
	p->body.room.mapfiles = d_set_new(0);

        READWORD(p->body.room.map->w);
        READWORD(p->body.room.map->h);

	READBYTE(bytebuf);
	while(bytebuf--) {
	    READDWORD(key);
	    READDWORD(dwordbuf);
	    if(dwordbuf > 0) {
		s = d_memory_new(dwordbuf);
		if(s == NULL)
		    return failure;
		i = readbuffer(nh->socket, s, dwordbuf);
		if(i == -1) goto error;
		if(i == 0) return failure;
	    } else
		s = NULL;

	    d_set_add(p->body.room.mapfiles, key, s);
	}

        p->body.room.map->map = d_memory_new(p->body.room.map->w*
					     p->body.room.map->h);
	d_memory_set(p->body.room.map->map, 0, p->body.room.map->w*
		                               p->body.room.map->h);
	i = readbuffer(nh->socket, (char *)p->body.room.map->map,
		       p->body.room.map->w*p->body.room.map->h);
	if(i == -1) goto error;
	if(i == 0) return failure;

        READDWORD(dwordbuf);
        if(dwordbuf > 0) {
            p->body.room.bgname = d_memory_new(dwordbuf);
            if(p->body.room.bgname == NULL)
                return failure;
            i = readbuffer(nh->socket, p->body.room.bgname, dwordbuf);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.room.bgname = NULL;
        p->body.room.bg = NULL;

        /* contents */
        p->body.room.contents = NULL;
        READDWORD(dwordbuf);
        if(dwordbuf > 0) {
            p->body.room.contents = d_set_new(0);

            while(dwordbuf-- > 0) {
                READWORD(wordbuf);
                d_set_add(p->body.room.contents, wordbuf, NULL);
            }
        }

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


bool net_writepack(nethandle_t *nh_, packet_t p)
{
    int i, j;
    byte bytebuf, *statebuf;
    word wordbuf;
    short shortbuf;
    dword dwordbuf, statelen, key;
    nh_t *nh = nh_;
    char *s;
    d_iterator_t it;

    i = write(nh->socket, &p.type, 1);
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
            i = writebuffer(nh->socket, p.body.event.auxdata,
			    p.body.event.auxlen);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }
        break;

    case PACK_LOGIN:
        WRITEDWORD(p.body.login.name.len, dwordbuf);
        if(p.body.login.name.len > 0) {
            i = writebuffer(nh->socket, (char *)p.body.login.name.data,
			    p.body.login.name.len);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }

        WRITEDWORD(p.body.login.password.len, dwordbuf);
        if(p.body.login.password.len > 0) {
            i = writebuffer(nh->socket, (char *)p.body.login.password.data,
			    p.body.login.password.len);
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
    case PACK_GETRESLIST:
        break;

    case PACK_GETFILE:
	WRITEDWORD(p.body.string.len, dwordbuf);
        if(p.body.string.len > 0) {
            i = writebuffer(nh->socket, (char *)p.body.string.data,
			    p.body.string.len);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }
	break;

    case PACK_FILE:
	WRITEDWORD(p.body.file.name.len, dwordbuf);
        if(p.body.string.len > 0) {
            i = writebuffer(nh->socket, (char *)p.body.file.name.data,
			    p.body.file.name.len);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }
	WRITEDWORD(p.body.file.length, dwordbuf);
	WRITEDWORD(p.body.file.checksum, dwordbuf);
	i = writebuffer(nh->socket, (char *)p.body.file.data, p.body.file.length);
	if(i == -1) goto error;
	if(i == 0) return failure;
	break;

    case PACK_RESLIST:
	WRITEWORD(p.body.reslist.length, wordbuf);
	for(j = 0; j < p.body.reslist.length; j++) {
	    WRITEDWORD(p.body.reslist.name[j].len, dwordbuf);

	    i = writebuffer(nh->socket, (char *)p.body.reslist.name[j].data,
			    p.body.reslist.name[j].len);
	    if(i == -1) goto error;
	    if(i == 0) return failure;

	    WRITEDWORD(p.body.reslist.checksum[j], dwordbuf);
	}
	break;

    case PACK_OBJECT:
        WRITEDWORD(strlen(p.body.object.name)+1, dwordbuf);
        if(strlen(p.body.object.name) > 0) {
            i = writebuffer(nh->socket, (char *)p.body.object.name,
			    strlen(p.body.object.name)+1);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }

        WRITEWORD(p.body.object.handle, wordbuf);
        WRITEWORD(p.body.object.location, wordbuf);

        /* physics */

        WRITEWORD(p.body.object.x, wordbuf);
        WRITEWORD(p.body.object.y, wordbuf);
        WRITEWORD(p.body.object.ax, shortbuf);
        WRITEWORD(p.body.object.ay, shortbuf);
        WRITEWORD(p.body.object.vx, shortbuf);
        WRITEWORD(p.body.object.vy, shortbuf);

        bytebuf = 0;
        bytebuf |= (p.body.object.onground == true) ? 1:0;
        WRITEBYTE(bytebuf);

        /* graphics */
        WRITEDWORD(strlen(p.body.object.spname)+1, dwordbuf);
        if(strlen(p.body.object.spname) > 0) {
            i = writebuffer(nh->socket, p.body.object.spname,
			    strlen(p.body.object.spname)+1);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }

        /* scripts */
        freezestate(p.body.object.luastate, &statebuf, &statelen);
        WRITEDWORD(statelen, dwordbuf);
        if(statelen > 0) {
            i = writebuffer(nh->socket, (char *)statebuf, statelen);
            if(i == -1) goto error;
            if(i == 0) return failure;
            d_memory_delete(statebuf);
        }
        statebuf = NULL;

        break;

    case PACK_ROOM:
        WRITEDWORD(strlen(p.body.room.name)+1, dwordbuf);
        if(strlen(p.body.room.name) > 0) {
            i = writebuffer(nh->socket, p.body.room.name,
			    strlen(p.body.room.name)+1);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }

        WRITEDWORD(strlen(p.body.room.owner)+1, dwordbuf);
        if(strlen(p.body.room.owner) > 0) {
            i = writebuffer(nh->socket, p.body.room.owner,
			    strlen(p.body.room.owner)+1);
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
	WRITEWORD(p.body.room.map->w, wordbuf);
	WRITEWORD(p.body.room.map->h, wordbuf);

	bytebuf = d_set_nelements(p.body.room.mapfiles);
	WRITEBYTE(bytebuf);

	d_iterator_reset(&it);
	while(key = d_set_nextkey(&it, p.body.room.mapfiles),
	      key != D_SET_INVALIDKEY) {
	    d_set_fetch(p.body.room.mapfiles, key, (void **)&s);
	    WRITEDWORD(key, dwordbuf);
	    WRITEDWORD(strlen(s)+1, dwordbuf);

	    i = writebuffer(nh->socket, s, strlen(s)+1);
            if(i == -1) goto error;
            if(i == 0) return failure;
	}

	i = writebuffer(nh->socket, (char *)p.body.room.map->map,
			p.body.room.map->w*p.body.room.map->h);
	if(i == -1) goto error;
	if(i == 0) return failure;

        WRITEDWORD(strlen(p.body.room.bgname)+1, dwordbuf);
        if(strlen(p.body.room.bgname) > 0) {
            i = writebuffer(nh->socket, p.body.room.bgname,
			    strlen(p.body.room.bgname)+1);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }

        /* contents */
        if(d_set_nelements(p.body.room.contents) > 0) {
            WRITEDWORD(d_set_nelements(p.body.room.contents), dwordbuf);
            d_iterator_reset(&it);
            while(key = d_set_nextkey(&it, p.body.room.contents),
                  key != D_SET_INVALIDKEY)
                WRITEWORD(key, wordbuf);
        } else
            WRITEDWORD(0, dwordbuf);
        
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


int readbuffer(int socket, char *data, dword len)
{
    int p;

    p = 0;
    while(p < len) {
	p += read(socket, data+p, len-p);
	if(p == -1) return -1;
	if(p == 0) return 0;
    }

    return p;
}


int writebuffer(int socket, char *data, dword len)
{
    int p;

    p = 0;
    while(p < len) {
	p += write(socket, data+p, len-p);
	if(p == -1) return -1;
	if(p == 0) return 0;
    }

    return p;
}

/* EOF network.c */
