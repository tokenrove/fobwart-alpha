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


bool net_syncevents(nethandle_t *nh, eventstack_t *evsk);
void net_close(nethandle_t *nh_);
bool net_login(nethandle_t *nh, char *uname, char *password,
               objhandle_t *localobj);
bool net_newclient(nethandle_t **nh_, char *servname, int port);
bool net_newserver(nethandle_t **nh_, int port);

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


bool net_syncevents(nethandle_t *nh, eventstack_t *evsk)
{
    bool status;
    packet_t p;

    p.type = PACK_EVENT;
    while(evsk_pop(evsk, &p.body.event))
        net_writepack(nh, p);

    p.type = PACK_FRAME;
    status = net_writepack(nh, p);
    if(status == failure) {
        d_error_debug(__FUNCTION__": write frame failed.\n");
        return failure;
    }
    while(status = net_readpack(nh, &p), status != failure &&
                                         p.type != PACK_FRAME) {
        if(p.type != PACK_EVENT)
            d_error_debug(__FUNCTION__": read event failed.\n");
        evsk_push(evsk, p.body.event);
    }
    if(status == failure) {
        d_error_debug(__FUNCTION__": read frame failed.\n");
        return failure;
    }
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


#define READBYTE(d) { i = read(nh->socket, &(d), 1); \
                      if(i == -1) goto error; \
                      if(i == 0) return failure; }

#define READWORD(d, t) { i = read(nh->socket, &(t), 2); \
                         if(i == -1) goto error; \
                         if(i == 0) return failure; \
                         (d) = ntohs((t)); }

#define READDWORD(d, t) { i = read(nh->socket, &(t), 4); \
                          if(i == -1) goto error; \
                          if(i == 0) return failure; \
                          (d) = ntohl((t)); }

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
    short shortbuf;
    dword dwordbuf;
    string_t *name, *password;
    dword statelen;
    nh_t *nh = nh_;

    i = read(nh->socket, &p->type, 1);
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
            i = readbuffer(nh->socket, p->body.event.auxdata,
			   p->body.event.auxlen);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.event.auxdata = NULL;
        break;

    case PACK_LOGIN:
        name = &p->body.login.name;
        READDWORD(name->len, dwordbuf);
        if(name->len > 0) {
            name->data = d_memory_new(name->len);
            if(name->data == NULL)
                return failure;
            i = readbuffer(nh->socket, (char *)name->data, name->len);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            name->data = NULL;

        password = &p->body.login.password;
        READDWORD(password->len, dwordbuf);
        if(password->len > 0) {
            password->data = d_memory_new(password->len);
            if(password->data == NULL)
                return failure;
            i = readbuffer(nh->socket, (char *)password->data, password->len);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            password->data = NULL;
        break;

    case PACK_AYUP:
    case PACK_GETOBJECT:
    case PACK_GETROOM:
        READWORD(p->body.handle, wordbuf);
        break;

    case PACK_FRAME:
    case PACK_GETRESLIST:
        break;

    case PACK_GETFILE:
	READDWORD(p->body.string.len, dwordbuf);
        if(p->body.string.len > 0) {
	    p->body.string.data = d_memory_new(p->body.string.len);
            i = readbuffer(nh->socket, (char *)p->body.string.data,
			   p->body.string.len);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }
	break;

    case PACK_FILE:
	READDWORD(p->body.file.name.len, dwordbuf);
        if(p->body.string.len > 0) {
	    p->body.file.name.data = d_memory_new(p->body.file.name.len);
            i = readbuffer(nh->socket, (char *)p->body.file.name.data,
			   p->body.file.name.len);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }
	READDWORD(p->body.file.length, dwordbuf);
	READDWORD(p->body.file.checksum, dwordbuf);
	p->body.file.data = d_memory_new(p->body.file.length);
	i = readbuffer(nh->socket, (char *)p->body.file.data,
		       p->body.file.length);
	if(i == -1) goto error;
	if(i == 0) return failure;
	break;

    case PACK_RESLIST:
	READWORD(p->body.reslist.length, wordbuf);
	p->body.reslist.name = d_memory_new(p->body.reslist.length*
					    sizeof(string_t));
	p->body.reslist.checksum = d_memory_new(p->body.reslist.length*
						sizeof(dword));

	for(j = 0; j < p->body.reslist.length; j++) {
	    READDWORD(p->body.reslist.name[j].len, dwordbuf);

	    p->body.reslist.name[j].data = d_memory_new(p->body.reslist.name[j].len);
	    i = readbuffer(nh->socket, (char *)p->body.reslist.name[j].data,
			   p->body.reslist.name[j].len);
	    if(i == -1) goto error;
	    if(i == 0) return failure;

	    printf("%s\n", p->body.reslist.name[j].data);

	    READDWORD(p->body.reslist.checksum[j], dwordbuf);
	}
	break;

    case PACK_OBJECT:
        READDWORD(dwordbuf, dwordbuf);
        if(dwordbuf > 0) {
            p->body.object.name = d_memory_new(dwordbuf);
            if(p->body.object.name == NULL)
                return failure;
            i = readbuffer(nh->socket, p->body.object.name, dwordbuf);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.object.name = NULL;

        READWORD(p->body.object.handle, wordbuf);
        READWORD(p->body.object.location, wordbuf);

        /* physics */
        READWORD(p->body.object.x, wordbuf);
        READWORD(p->body.object.y, wordbuf);
        READWORD(p->body.object.ax, shortbuf);
        READWORD(p->body.object.ay, shortbuf);
        READWORD(p->body.object.vx, shortbuf);
        READWORD(p->body.object.vy, shortbuf);

        READBYTE(bytebuf);
        p->body.object.onground = (bytebuf&1) ? true : false;

        /* statistics */
        READWORD(p->body.object.hp, wordbuf);
        READWORD(p->body.object.maxhp, wordbuf);

        /* graphics */
        READDWORD(dwordbuf, dwordbuf);
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
        READDWORD(statelen, dwordbuf);
        if(statelen > 0) {
            p->body.object.statebuf = d_memory_new(statelen);
            if(p->body.object.statebuf == NULL) return failure;

            i = readbuffer(nh->socket, p->body.object.statebuf, statelen);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }
        break;

    case PACK_ROOM:
        READDWORD(dwordbuf, dwordbuf);
        if(dwordbuf > 0) {
            p->body.room.name = d_memory_new(dwordbuf);
            if(p->body.room.name == NULL)
                return failure;
            i = readbuffer(nh->socket, p->body.room.name, dwordbuf);
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
            i = readbuffer(nh->socket, p->body.room.mapname, dwordbuf);
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
            i = readbuffer(nh->socket, p->body.room.bgname, dwordbuf);
            if(i == -1) goto error;
            if(i == 0) return failure;
        } else
            p->body.room.bgname = NULL;
        p->body.room.bg = NULL;

        /* contents */
        p->body.room.contents = NULL;
        READDWORD(dwordbuf, dwordbuf);
        if(dwordbuf > 0) {
            p->body.room.contents = d_set_new(0);

            while(dwordbuf-- > 0) {
                READWORD(wordbuf, wordbuf);
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

        /* statistics */
        WRITEWORD(p.body.object.hp, wordbuf);
        WRITEWORD(p.body.object.maxhp, wordbuf);

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

        WRITEWORD(p.body.room.handle, wordbuf);

        /* physics */
        WRITEWORD(p.body.room.gravity, wordbuf);
        bytebuf = 0;
        bytebuf |= (p.body.room.islit == true) ? 1:0;
        WRITEBYTE(bytebuf);

        /* tilemaps */
        WRITEDWORD(strlen(p.body.room.mapname)+1, dwordbuf);
        if(strlen(p.body.room.mapname) > 0) {
            i = writebuffer(nh->socket, p.body.room.mapname,
			    strlen(p.body.room.mapname)+1);
            if(i == -1) goto error;
            if(i == 0) return failure;
        }

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
