/*
 * servdata.c
 * Julian Squires <tek@wiw.org> / 2001
 */

#include "fobwart.h"
#include "fobdb.h"
#include "fobnet.h"
#include "fobserv.h"

#include <dentata/error.h>
#include <dentata/file.h>
#include <dentata/memory.h>
#include <dentata/random.h>
#include <dentata/util.h>


bool loadservdata(serverdata_t *sd);
bool getobject(serverdata_t *sd, objhandle_t key);
bool loadfile(char *name, byte **data, dword *len);


bool loadfile(char *name, byte **data, dword *len)
{
    d_file_t *file;

    file = d_file_open(name);
    if(!file) return failure;

    d_file_seek(file, 0, end);
    *len = d_file_tell(file);
    d_file_seek(file, 0, beginning);

    *data = d_memory_new(*len);
    if(d_file_read(file, *data, *len) != *len) {
	d_error_debug("Read didn't match %d\n", *len);
    }

    d_file_close(file);

    return success;
}


bool getobject(serverdata_t *sd, objhandle_t handle)
{
    object_t *o;
    bool status;

    o = d_memory_new(sizeof(object_t));
    if(o == NULL) return failure;
    o->handle = handle;
    objectdb_get(sd->objectdb, handle, o);
    status = d_set_add(sd->ws.objs, o->handle, (void *)o);
    if(status == failure) return failure;

    status = deskelobject(o);
    return status;
}


bool reloaddbs(serverdata_t *sd)
{
    bool status;

    closelogindb(sd->logindb);
    status = loadlogindb(sd->logindb);
    if(status != success) return failure;

    /* FIXME sync object db here */
    closeobjectdb(sd->objectdb);
    status = loadobjectdb(sd->objectdb);
    if(status != success) return failure;

    /* FIXME: sync room db here */
    closeroomdb(sd->roomdb);
    status = loadroomdb(sd->roomdb);
    if(status != success) return failure;

    return success;
}


bool loadservdata(serverdata_t *sd)
{
    dword key, key2;
    room_t *room;
    object_t *o;
    d_iterator_t it, it2;

    checksuminit();
    memset(&sd->reslist, 0, sizeof(sd->reslist));

    /* Setup some initial resources required. */
    reslist_add(&sd->reslist, DATADIR, "defobj", ".luc");
    reslist_add(&sd->reslist, DATADIR, "light", ".pal");
    reslist_add(&sd->reslist, DATADIR, "dark", ".pal");
    reslist_add(&sd->reslist, DATADIR, "def", ".fnt");
    reslist_add(&sd->reslist, DATADIR, "slant", ".fnt");

    /* load room db */
    sd->ws.rooms = d_set_new(0);
    if(sd->ws.rooms == NULL) return failure;

    /* Load each room */
    roomdb_getall(sd->roomdb, sd->ws.rooms);

    /* load object db */
    sd->ws.objs = d_set_new(0);
    if(sd->ws.objs == NULL) return failure;

    d_iterator_reset(&it);
    while(key = d_set_nextkey(&it, sd->ws.rooms), key != D_SET_INVALIDKEY) {

        d_set_fetch(sd->ws.rooms, key, (void **)&room);
	deskelroom(room);

	/* Generate reslist entries for this room's data. */
	reslist_add(&sd->reslist, DATADIR, room->bgname, ".pcx");
	reslist_add(&sd->reslist, DATADIR, room->mapname, ".map");

        d_iterator_reset(&it2);
        while(key2 = d_set_nextkey(&it2, room->contents),
              key2 != D_SET_INVALIDKEY) {
            if(getobject(sd, key2) != success) {
                d_error_debug(__FUNCTION__": failed to get object %d\n", key2);
                return failure;
            }

            /* consistency checks */
            d_set_fetch(sd->ws.objs, key2, (void **)&o);
            if(o->location != key) {
                d_error_debug(__FUNCTION__": note: object %d had bad "
                              "location %d.\n", key2, o->location);
                o->location = key;
            }

	    /* Generate reslist entries for this object. */
	    reslist_add(&sd->reslist, DATADIR, o->spname, ".spr");
	    reslist_add(&sd->reslist, DATADIR, o->spname, ".luc");
        }
    }

    return success;
}

/* EOF servdata.c */
