/*
 * db.c ($Id$)
 * Julian Squires <tek@wiw.org> / 2001
 *
 * Database interface routines.
 */


#include <dentata/types.h>
#include <dentata/image.h>
#include <dentata/font.h>
#include <dentata/sprite.h>
#include <dentata/pcx.h>
#include <dentata/tilemap.h>
#include <dentata/manager.h>
#include <dentata/memory.h>
#include <dentata/file.h>
#include <dentata/raster.h>
#include <dentata/error.h>
#include <dentata/sample.h>
#include <dentata/audio.h>
#include <dentata/s3m.h>
#include <dentata/set.h>
#include <dentata/random.h>
#include <dentata/util.h>

#include <sys/types.h>
#include <limits.h>
#include <fcntl.h>
#include <db.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>

#include <lua.h>

#include "fobwart.h"
#include "fobdb.h"


#define LOGINDB "data/login.db"
#define OBJECTDB "data/object.db"
#define ROOMDB "data/room.db"

typedef struct dbinternal_s {
    DB *p;
} dbinternal_t;


dbhandle_t *newdbh(void);
void deletedbh(dbhandle_t *db_);

bool loadobjectdb(dbhandle_t *db_);
void closeobjectdb(dbhandle_t *db_);
bool objectdb_get(dbhandle_t *db_, objhandle_t handle, object_t *o);
bool objectdb_put(dbhandle_t *db_, objhandle_t handle, object_t *o);
bool objectdb_remove(dbhandle_t *dbh_, objhandle_t handle);

bool loadroomdb(dbhandle_t *db_);
void closeroomdb(dbhandle_t *db_);
bool roomdb_get(dbhandle_t *db_, roomhandle_t handle, room_t *room);
bool roomdb_put(dbhandle_t *db_, roomhandle_t handle, room_t *room);
bool roomdb_remove(dbhandle_t *dbh_, roomhandle_t handle);

bool loadlogindb(dbhandle_t *db_);
void closelogindb(dbhandle_t *db_);
bool verifylogin(dbhandle_t *db_, char *name, char *pw, objhandle_t *object,
		 byte *class);
bool logindb_remove(dbhandle_t *db_, char *handle);
bool logindb_put(dbhandle_t *db_, char *handle, loginrec_t *lrec);
bool logindb_get(dbhandle_t *db_, char *handle, loginrec_t *lrec);

static void pack(DBT *dst, char *fmt, ...);
static void unpack(DBT *src, char *fmt, ...);
static void objencode(object_t *obj, DBT *data);
static void objdecode(object_t *obj, DBT *data);
static void roomencode(room_t *room, DBT *data);
static void roomdecode(room_t *room, DBT *data);
static int bt_handle_cmp(DB *dbp, const DBT *a_, const DBT *b_);


dbhandle_t *newdbh(void)
{
    dbinternal_t *db;

    db = malloc(sizeof(dbinternal_t));
    return (dbhandle_t *)db;
}


void deletedbh(dbhandle_t *db_)
{
    dbinternal_t *db = db_;

    d_memory_delete(db);
    return;
}


bool loadlogindb(dbhandle_t *db_)
{
    int i;
    dbinternal_t *db = db_;

    i = db_create(&db->p, NULL, 0);
    if(i != 0) {
        d_error_debug(__FUNCTION__": db_create: %s\n", db_strerror(i));
        return failure;
    }

    i = db->p->open(db->p, LOGINDB, NULL, DB_BTREE, DB_CREATE, 0664);
    if(i != 0) {
        db->p->err(db->p, i, "%s", LOGINDB);
        return failure;
    }

    return success;
}


void closelogindb(dbhandle_t *db_)
{
    dbinternal_t *db = db_;

    db->p->close(db->p, 0);
    return;
}


bool verifylogin(dbhandle_t *db, char *name, char *pw, objhandle_t *object,
		 byte *class)
{
    loginrec_t loginrec;
    bool status;

    if(name == NULL || pw == NULL) return failure;

    status = logindb_get(db, name, &loginrec);
    if(status != success) return failure;

    if(strcmp(loginrec.password, pw) != 0)
        return failure;

    if(object) *object = loginrec.object;
    if(class) *class = loginrec.class;

    return success;
}


bool logindb_get(dbhandle_t *db_, char *handle, loginrec_t *lrec)
{
    DBT key, data;
    dbinternal_t *db = db_;
    int i;

    d_memory_set(&key, 0, sizeof(key));
    d_memory_set(&data, 0, sizeof(data));
    key.size = strlen(handle)+1;
    key.data = handle;

    i = db->p->get(db->p, NULL, &key, &data, 0);
    if(i != 0) {
        db->p->err(db->p, i, "dbp->get");
        return failure;
    }
    unpack(&data, "swb", &lrec->password, &lrec->object, &lrec->class);
    return success;
}


bool logindb_put(dbhandle_t *db_, char *handle, loginrec_t *lrec)
{
    DBT key, data;
    dbinternal_t *db = db_;
    int i;

    d_memory_set(&key, 0, sizeof(key));
    d_memory_set(&data, 0, sizeof(data));
    key.size = strlen(handle)+1;
    key.data = handle;

    pack(&data, "swb", lrec->password, lrec->object, lrec->class);

    i = db->p->put(db->p, NULL, &key, &data, 0);
    if(i != 0) {
        db->p->err(db->p, i, "dbp->put");
        return failure;
    }

    d_memory_delete(data.data);
    return success;
}


bool logindb_remove(dbhandle_t *db_, char *handle)
{
    DBT key;
    dbinternal_t *db = db_;
    int i;

    d_memory_set(&key, 0, sizeof(key));
    key.size = strlen(handle)+1;
    key.data = handle;

    i = db->p->del(db->p, NULL, &key, 0);
    if(i != 0) {
        db->p->err(db->p, i, "dbp->del");
        return failure;
    }

    return success;
}


bool loadobjectdb(dbhandle_t *db_)
{
    int i;
    dbinternal_t *db = db_;

    i = db_create(&db->p, NULL, 0);
    if(i != 0) {
        d_error_debug(__FUNCTION__": db_create: %s\n", db_strerror(i));
        return failure;
    }

    i = db->p->set_bt_compare(db->p, bt_handle_cmp);
    if(i != 0) {
        db->p->err(db->p, i, "%s", "set_bt_compare");
        return failure;
    }

    i = db->p->open(db->p, OBJECTDB, NULL, DB_BTREE, DB_CREATE, 0664);
    if(i != 0) {
        db->p->err(db->p, i, "%s", OBJECTDB);
        return failure;
    }

    return success;
}


void closeobjectdb(dbhandle_t *db_)
{
    dbinternal_t *db = db_;

    db->p->close(db->p, 0);
    return;
}


bool objectdb_get(dbhandle_t *db_, objhandle_t handle, object_t *o)
{
    DBT key, data;
    dbinternal_t *db = db_;
    int i;

    d_memory_set(&key, 0, sizeof(key));
    d_memory_set(&data, 0, sizeof(data));
    key.size = sizeof(objhandle_t);
    key.data = &handle;

    i = db->p->get(db->p, NULL, &key, &data, 0);
    if(i != 0) {
        db->p->err(db->p, i, "dbp->get");
        return failure;
    }

    objdecode(o, &data);
    return success;
}


bool objectdb_put(dbhandle_t *db_, objhandle_t handle, object_t *o)
{
    DBT key, data;
    dbinternal_t *db = db_;
    int i;

    d_memory_set(&key, 0, sizeof(key));
    d_memory_set(&data, 0, sizeof(data));
    key.size = sizeof(objhandle_t);
    key.data = &handle;

    objencode(o, &data);

    i = db->p->put(db->p, NULL, &key, &data, 0);
    if(i != 0) {
        db->p->err(db->p, i, "dbp->put");
        return failure;
    }

    d_memory_delete(data.data);
    return success;
}


bool objectdb_remove(dbhandle_t *db_, objhandle_t handle)
{
    DBT key;
    dbinternal_t *db = db_;
    int i;

    d_memory_set(&key, 0, sizeof(key));
    key.size = sizeof(objhandle_t);
    key.data = &handle;

    i = db->p->del(db->p, NULL, &key, 0);
    if(i != 0) {
        db->p->err(db->p, i, "dbp->del");
        return failure;
    }

    return success;
}


#define OBJPACKSTRING "swwwwwwwbsB"

void objencode(object_t *obj, DBT *data)
{
    byte bytebuf;
    byte *statebuf;
    dword statelen;

    bytebuf = 0;
    bytebuf |= (obj->onground == true)?1:0;

    freezestate(obj->luastate, &statebuf, &statelen);
    pack(data, OBJPACKSTRING, obj->name, obj->location, obj->x, obj->y,
         obj->ax, obj->ay, obj->vx, obj->vy, bytebuf, obj->spname,
	 statelen, statebuf);
    return;
}


void objdecode(object_t *obj, DBT *data)
{
    byte bytebuf;
    char *statebuf;

    unpack(data, OBJPACKSTRING, &obj->name, &obj->location, &obj->x, &obj->y,
           &obj->ax, &obj->ay, &obj->vx, &obj->vy, &bytebuf, &obj->spname,
	   &statebuf);

    deskelobject(obj);
    meltstate(obj->luastate, statebuf);
    d_memory_delete(statebuf);
    statebuf = NULL;

    obj->onground = (bytebuf&1)?true:false;
    return;
}


bool loadroomdb(dbhandle_t *db_)
{
    int i;
    dbinternal_t *db = db_;

    i = db_create(&db->p, NULL, 0);
    if(i != 0) {
        d_error_debug(__FUNCTION__": db_create: %s\n", db_strerror(i));
        return failure;
    }

    i = db->p->set_bt_compare(db->p, bt_handle_cmp);
    if(i != 0) {
        db->p->err(db->p, i, "%s", "set_bt_compare");
        return failure;
    }

    i = db->p->open(db->p, ROOMDB, NULL, DB_BTREE, DB_CREATE, 0664);
    if(i != 0) {
        db->p->err(db->p, i, "%s", ROOMDB);
        return failure;
    }

    return success;
}


void closeroomdb(dbhandle_t *db_)
{
    dbinternal_t *db = db_;

    db->p->close(db->p, 0);
    return;
}


bool roomdb_get(dbhandle_t *db_, roomhandle_t handle, room_t *room)
{
    DBT key, data;
    dbinternal_t *db = db_;
    int i;

    d_memory_set(&key, 0, sizeof(key));
    d_memory_set(&data, 0, sizeof(data));
    key.size = sizeof(roomhandle_t);
    key.data = &handle;

    i = db->p->get(db->p, NULL, &key, &data, 0);
    if(i != 0) {
        db->p->err(db->p, i, "dbp->get");
        return failure;
    }

    roomdecode(room, &data);
    return success;
}



bool roomdb_getall(dbhandle_t *db_, d_set_t *rooms)
{
    DBT key, data;
    dbinternal_t *db = db_;
    int i;
    room_t *room;
    DBC *dbc;
    bool status;

    d_memory_set(&key, 0, sizeof(key));
    d_memory_set(&data, 0, sizeof(data));

    i = db->p->cursor(db->p, NULL, &dbc, 0);
    if(i != 0) {
        db->p->err(db->p, i, "dbp->cursor");
        return failure;
    }

    while(dbc->c_get(dbc, &key, &data, DB_NEXT) != DB_NOTFOUND) {

	room = d_memory_new(sizeof(room_t));
	if(room == NULL) return failure;

	room->handle = *(roomhandle_t *)key.data;

	status = d_set_add(rooms, room->handle, (void *)room);
	if(status == failure) return failure;

	roomdecode(room, &data);
    }

    dbc->c_close(dbc);
    return success;
}


bool roomdb_put(dbhandle_t *db_, roomhandle_t handle, room_t *room)
{
    DBT key, data;
    dbinternal_t *db = db_;
    int i;

    d_memory_set(&key, 0, sizeof(key));
    d_memory_set(&data, 0, sizeof(data));
    key.size = sizeof(roomhandle_t);
    key.data = &handle;

    roomencode(room, &data);

    i = db->p->put(db->p, NULL, &key, &data, 0);
    if(i != 0) {
        db->p->err(db->p, i, "dbp->put");
        return failure;
    }

    d_memory_delete(data.data);
    return success;
}


bool roomdb_remove(dbhandle_t *db_, roomhandle_t handle)
{
    DBT key;
    dbinternal_t *db = db_;
    int i;

    d_memory_set(&key, 0, sizeof(key));
    key.size = sizeof(roomhandle_t);
    key.data = &handle;

    i = db->p->del(db->p, NULL, &key, 0);
    if(i != 0) {
        db->p->err(db->p, i, "dbp->del");
        return failure;
    }

    return success;
}


#define ROOMPACKSTRING "sswbsSkwwwwwwSsB"

void roomencode(room_t *room, DBT *data)
{
    byte bytebuf;

    bytebuf = 0;
    bytebuf |= (room->islit == true)?1:0;

    pack(data, ROOMPACKSTRING, room->name, room->owner, room->gravity,
	 bytebuf, room->bgname, room->contents, room->exits[0],
	 room->exits[1], room->exits[2], room->exits[3], room->map->w,
	 room->map->h, room->mapfiles, room->map->w*room->map->h,
	 room->map->map);
    return;
}


void roomdecode(room_t *room, DBT *data)
{
    byte bytebuf;

    room->map = d_memory_new(sizeof(d_tilemap_t));
    d_memory_set(room->map, 0, sizeof(d_tilemap_t));

    unpack(data, ROOMPACKSTRING, &room->name, &room->owner, &room->gravity,
	   &bytebuf, &room->bgname, &room->contents, &room->exits[0],
	   &room->exits[1], &room->exits[2], &room->exits[3], &room->map->w,
	   &room->map->h, &room->mapfiles, &room->map->map);
    room->islit = (bytebuf&1)?true:false;
    return;
}


void pack(DBT *dst, char *fmt, ...)
{
    byte *d;
    word wordbuf;
    dword dwordbuf, key;
    char *s, *oldfmt;
    d_set_t *set;
    d_iterator_t it;
    va_list ap;

    dst->size = 0;
    dst->data = NULL;
    ap = va_start(ap, fmt);
    oldfmt = fmt;

    while(*fmt) {
        switch(*fmt) {
        case 's': /* string */
            s = va_arg(ap, char *);
            dst->size += strlen(s)+1;
            break;

        case 'b': /* byte */
	    (void)va_arg(ap, byte);
            dst->size += sizeof(byte);
            break;

        case 'w': /* word */
	    (void)va_arg(ap, word);
            dst->size += sizeof(word);
            break;

        case 'd': /* dword */
	    (void)va_arg(ap, dword);
            dst->size += sizeof(dword);
            break;

        case 'S': /* set */
	    fmt++;
	    set = va_arg(ap, d_set_t *);
	    switch(*fmt) {
	    case 'k': /* keys only */
		dst->size += (d_set_nelements(set)+1)*sizeof(dword);
		break;

	    case 's': /* strings */
		d += sizeof(dword);
		d_iterator_reset(&it);
		while(key = d_set_nextkey(&it, set), key != D_SET_INVALIDKEY) {
		    dst->size += sizeof(dword);
		    d_set_fetch(set, key, (void **)&s);
		    dst->size += strlen(s)+1;
		}
		break;

	    default:
		d_error_debug(__FUNCTION__": got invalid set pack character ``S%c''.\n",
			      *fmt);
	    }
	    break;

	case 'B': /* byte array */
	    dwordbuf = va_arg(ap, dword);
	    (void)va_arg(ap, byte *);
	    dst->size += dwordbuf + sizeof(dword);
	    break;

        default:
            d_error_debug(__FUNCTION__": got invalid pack character ``%c''.\n",
                          *fmt);
        }
        fmt++;
    }

    fmt = oldfmt;
    va_end(ap);
    va_start(ap, fmt);
    dst->data = malloc(dst->size);
    d = dst->data;

    while(*fmt) {
        switch(*fmt) {
        case 's': /* string */
            s = va_arg(ap, char *);
            memcpy(d, s, strlen(s)+1);
            d += strlen(s)+1;
            break;

        case 'b': /* byte */
	    *d = va_arg(ap, byte);
            d++;
            break;

        case 'w': /* word */
            wordbuf = va_arg(ap, word);
            wordbuf = htons(wordbuf);
            memcpy(d, &wordbuf, sizeof(word));
            d += sizeof(word);
            break;

        case 'd': /* dword */
            dwordbuf = va_arg(ap, dword);
            dwordbuf = htonl(dwordbuf);
            memcpy(d, &dwordbuf, sizeof(dword));
            d += sizeof(dword);
            break;

        case 'S': /* set */
            set = va_arg(ap, d_set_t *);
	    fmt++;
	    switch(*fmt) {
	    case 'k': /* keys only */
		key = d_set_nelements(set);
		key = htonl(key);
		memcpy(d, &key, sizeof(dword));
		d += sizeof(dword);
		d_iterator_reset(&it);
		while(key = d_set_nextkey(&it, set), key != D_SET_INVALIDKEY) {
		    key = htonl(key);
		    memcpy(d, &key, sizeof(dword));
		    d += sizeof(dword);
		}
		break;

	    case 's': /* strings */
		key = d_set_nelements(set);
		key = htonl(key);
		memcpy(d, &key, sizeof(dword));
		d += sizeof(dword);
		d_iterator_reset(&it);
		while(key = d_set_nextkey(&it, set), key != D_SET_INVALIDKEY) {
		    d_set_fetch(set, key, (void **)&s);
		    key = htonl(key);
		    memcpy(d, &key, sizeof(dword));
		    d += sizeof(dword);
		    memcpy(d, s, strlen(s)+1);
		    d += strlen(s)+1;
		}
		break;
	    }
	    break;

	case 'B': /* byte array */
	    dwordbuf = va_arg(ap, dword);
            s = va_arg(ap, char *);
	    key = dwordbuf;
	    dwordbuf = htonl(key);
            memcpy(d, &dwordbuf, sizeof(dword));
	    d += sizeof(dword);
            memcpy(d, s, key);
	    d += key;
	    break;

        default:
            d_error_debug(__FUNCTION__": got invalid pack character ``%c''.\n",
                          *fmt);
        }
        fmt++;
    }

    va_end(ap);
    return;
}


void unpack(DBT *src, char *fmt, ...)
{
    byte *d, *bp;
    word *wp;
    dword *dwp, key, nitems;
    char **s, *stmp;
    int slen;
    d_set_t **set;
    va_list ap;

    d = src->data;
    va_start(ap, fmt);

    while(*fmt) {
        switch(*fmt) {
        case 's': /* string */
            s = va_arg(ap, char **);
            slen = strlen((char *)d)+1;
            *s = malloc(slen);
            memcpy(*s, d, slen);
            d += slen;
            break;

        case 'b': /* byte */
            bp = va_arg(ap, byte *);
            *bp = *d;
            d++;
            break;

        case 'w': /* word */
            wp = va_arg(ap, word *);
            memcpy(wp, d, sizeof(word));
            *wp = ntohs(*wp);
            d += sizeof(word);
            break;

        case 'd': /* dword */
            dwp = va_arg(ap, dword *);
            memcpy(dwp, d, sizeof(dword));
            *dwp = ntohl(*dwp);
            d += sizeof(dword);
            break;

        case 'S': /* set */
            set = va_arg(ap, d_set_t **);
            *set = d_set_new(0);
	    ++fmt;
	    switch(*fmt) {
	    case 'k': /* keys only */
		memcpy(&nitems, d, sizeof(dword));
		nitems = ntohl(nitems);
		d += sizeof(dword);
		while(nitems--) {
		    memcpy(&key, d, sizeof(dword));
		    d += sizeof(dword);
		    key = ntohl(key);
		    d_set_add(*set, key, NULL);
		}
		break;

	    case 's': /* strings */
		memcpy(&nitems, d, sizeof(dword));
		nitems = ntohl(nitems);
		d += sizeof(dword);
		while(nitems--) {
		    memcpy(&key, d, sizeof(dword));
		    key = ntohl(key);
		    d += sizeof(dword);
		    slen = strlen((char *)d)+1;
		    stmp = malloc(slen);
		    memcpy(stmp, d, slen);
		    d_set_add(*set, key, stmp);
		    d += slen;
		}
		break;
	    }
	    break;

	case 'B': /* byte array */
            memcpy(&key, d, sizeof(dword));
            key = ntohl(key);
            d += sizeof(dword);
            s = va_arg(ap, char **);
            *s = malloc(key);
            memcpy(*s, d, key);
            d += key;
	    break;

        default:
            d_error_debug(__FUNCTION__": got invalid pack character ``%c''.\n",
                          *fmt);
        }
        fmt++;
    }

    va_end(ap);
    return;
}


int bt_handle_cmp(DB *dbp, const DBT *a_, const DBT *b_)
{
    word a, b;

    memcpy(&a, a_->data, sizeof(word));
    memcpy(&b, b_->data, sizeof(word));
    return (int)a - (int)b;
}

/* EOF db.c */
