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

#include <sys/types.h>
#include <limits.h>
#include <fcntl.h>
#include <db.h>
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


void pack(DBT *dst, char *fmt, ...);
void unpack(DBT *src, char *fmt, ...);
void objencode(object_t *obj, DBT *data);
void objdecode(object_t *obj, DBT *data);
int bt_handle_cmp(DB *dbp, const DBT *a_, const DBT *b_);
bool objectdb_get(dbhandle_t *db_, word handle, object_t *o);
bool loadlogindb(dbhandle_t *db_);
void closelogindb(dbhandle_t *db_);
bool verifylogin(dbhandle_t *db_, char *name, char *pw, objhandle_t *object);
bool loadobjectdb(dbhandle_t *db_);
void closeobjectdb(dbhandle_t *db_);
dbhandle_t *newdbh(void);
void deletedbh(dbhandle_t *db_);


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


bool verifylogin(dbhandle_t *db_, char *name, char *pw, objhandle_t *object)
{
    DBT key, data;
    loginrec_t loginrec;
    word wordbuf;
    int i;
    dbinternal_t *db = db_;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    memset(&loginrec, 0, sizeof(loginrec));

    key.data = name;
    key.size = strlen(name)+1;

    i = db->p->get(db->p, NULL, &key, &data, 0);
    if(i != 0)
        return failure;
    loginrec.password = data.data;
    memcpy(&wordbuf, (char *)data.data+strlen(loginrec.password)+1,
           sizeof(wordbuf));
    loginrec.object = ntohs(wordbuf);

    if(strcmp(loginrec.password, pw) != 0)
        return failure;

    *object = loginrec.object;
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


bool objectdb_get(dbhandle_t *db_, word handle, object_t *o)
{
    DBT key, data;
    dbinternal_t *db = db_;
    int i;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.size = sizeof(objhandle_t);
    key.data = malloc(key.size);
    memcpy(key.data, &handle, key.size);

    i = db->p->get(db->p, NULL, &key, &data, 0);
    if(i != 0) {
        db->p->err(db->p, i, "dbp->get");
        free(key.data);
        return failure;
    }

    objdecode(o, &data);
    free(key.data);
    return success;
}


void objencode(object_t *obj, DBT *data)
{
    byte bytebuf;

    bytebuf = 0;
    bytebuf |= (obj->onground == true)?1:0;

    pack(data, "swwwwwwwbwws", obj->name, obj->location, obj->x, obj->y,
         obj->ax, obj->ay, obj->vx, obj->vy, bytebuf, obj->hp, obj->maxhp,
         obj->spname);
    return;
}


void objdecode(object_t *obj, DBT *data)
{
    byte bytebuf;

    unpack(data, "swwwwwwwbwws", &obj->name, &obj->location, &obj->x, &obj->y,
           &obj->ax, &obj->ay, &obj->vx, &obj->vy, &bytebuf, &obj->hp,
           &obj->maxhp, &obj->spname);
    obj->onground = (bytebuf&1)?true:false;
    return;
}


void pack(DBT *dst, char *fmt, ...)
{
    byte *d, *bp;
    word *wp;
    dword *dwp;
    char **s, *oldfmt;
    void **arg, **oldarg;

    dst->size = 0;
    dst->data = NULL;
    arg = (void **)&fmt;
    arg++; oldarg = arg;
    oldfmt = fmt;

    while(*fmt) {
        switch(*fmt) {
        case 's': /* string */
            s = (char **)arg;
            dst->size += strlen(*s)+1;
            break;

        case 'b': /* byte */
            dst->size += sizeof(byte);
            break;

        case 'w': /* word */
            dst->size += sizeof(word);
            break;

        case 'd': /* dword */
            dst->size += sizeof(dword);
            break;

        default:
            d_error_debug(__FUNCTION__": got invalid pack character ``%c''.\n",
                          *fmt);
        }
        fmt++;
        arg++;
    }

    fmt = oldfmt;
    arg = oldarg;
    dst->data = malloc(dst->size);
    d = dst->data;

    while(*fmt) {
        switch(*fmt) {
        case 's': /* string */
            s = (char **)arg;
            memcpy(d, *s, strlen(*s)+1);
            d += strlen(*s)+1;
            break;

        case 'b': /* byte */
            bp = (byte *)arg;
            *d = *(byte *)bp;
            d++;
            break;

        case 'w': /* word */
            wp = (word *)arg;
            *wp = htons(*wp);
            memcpy(d, wp, sizeof(word));
            d += sizeof(word);
            break;

        case 'd': /* dword */
            dwp = (dword *)arg;
            *dwp = htonl(*dwp);
            memcpy(d, dwp, sizeof(dword));
            d += sizeof(dword);
            break;

        default:
            d_error_debug(__FUNCTION__": got invalid pack character ``%c''.\n",
                          *fmt);
        }
        fmt++;
        arg++;
    }

    return;
}


void unpack(DBT *src, char *fmt, ...)
{
    byte *d, **bp;
    word **wp;
    dword **dwp;
    void **arg;
    char ***s;
    int slen;

    d = src->data;
    arg = (void **)&fmt;
    arg++;

    while(*fmt) {
        switch(*fmt) {
        case 's': /* string */
            s = (char ***)arg;
            slen = strlen((char *)d)+1;
            **s = malloc(slen);
            memcpy(**s, d, slen);
            d += slen;
            break;

        case 'b': /* byte */
            bp = (byte **)arg;
            **bp = *d;
            d++;
            break;

        case 'w': /* word */
            wp = (word **)arg;
            memcpy(*wp, d, sizeof(word));
            **wp = ntohs(**wp);
            d += sizeof(word);
            break;

        case 'd': /* dword */
            dwp = (dword **)arg;
            memcpy(*dwp, d, sizeof(dword));
            **dwp = ntohl(**dwp);
            d += sizeof(dword);
            break;

        default:
            d_error_debug(__FUNCTION__": got invalid pack character ``%c''.\n",
                          *fmt);
        }
        fmt++;
        arg++;
    }
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
