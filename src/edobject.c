/* 
 * edobject.c ($Id$)
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
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

#include <stdio.h>
#include <stdlib.h>

#include <lua.h>

#include "fobwart.h"
#include "fobdb.h"

#define PROGNAME "edobject"


int main(int argc, char **argv)
{
    enum { HELP = 0, CREATE, ADD, REMOVE, VIEW, CHANGE } command = HELP;
    objhandle_t key;
    object_t object;
    int i, comspec = 0;
    word wordbuf;
    char *field, *value;
    dbhandle_t *dbh;
    bool status;

    d_memory_set(&object, 0, sizeof(object));
    field = value = NULL;
    key = 0;

    for(i = 1; i < argc; i++) {
        if(argv[i][0] == '-') {
        } else if(comspec == 0) {
            if     (strcmp(argv[i], "help") == 0) command = HELP;
            else if(strcmp(argv[i], "create") == 0) command = CREATE;
            else if(strcmp(argv[i], "add") == 0) command = ADD;
            else if(strcmp(argv[i], "remove") == 0) command = REMOVE;
            else if(strcmp(argv[i], "view") == 0) command = VIEW;
            else if(strcmp(argv[i], "change") == 0) command = CHANGE;
            else
                d_error_fatal("Unknown command. (try ``help'')\n");
            comspec++;
        } else {
            if(command == ADD) {
                if(comspec == 1) {
                    key = atoi(argv[i]);
                } else if(comspec == 2) {
                    object.name = argv[i];
                } else if(comspec == 3) {
                    object.location = atoi(argv[i]);
                } else if(comspec == 4) {
                    object.x = atoi(argv[i]);
                } else if(comspec == 5) {
                    object.y = atoi(argv[i]);
                } else if(comspec == 6) {
                    object.ax = atoi(argv[i]);
                } else if(comspec == 7) {
                    object.ay = atoi(argv[i]);
                } else if(comspec == 8) {
                    object.vx = atoi(argv[i]);
                } else if(comspec == 9) {
                    object.vy = atoi(argv[i]);
                } else if(comspec == 10) {
                    object.onground = (strcmp(argv[i], "false"))?true:false;
                } else if(comspec == 11) {
                    object.hp = atoi(argv[i]);
                } else if(comspec == 12) {
                    object.maxhp = atoi(argv[i]);
                } else if(comspec == 13) {
                    object.spname = argv[i];
                } else {
                    d_error_fatal("Too many arguments.\n");
                }
            } else if(command == CHANGE) {
                if(comspec == 1) {
                    key = atoi(argv[i]);
                } else if(comspec == 2) {
                    field = argv[i];
                } else if(comspec == 3) {
                    value = argv[i];
                } else {
                    d_error_fatal("Too many arguments.\n");
                }
            } else if(command == REMOVE ||
                      command == VIEW) {
                if(comspec == 1) {
                    key = atoi(argv[i]);
                } else {
                    d_error_fatal("Too many arguments.\n");
                }
            } else
                d_error_fatal("Bad argument to command. (try ``help'')\n");
            comspec++;
        }
    }

    if(command == CHANGE && (field == NULL || value == NULL))
        command = HELP;

    dbh = newdbh();
    if(command == CREATE)
        d_error_fatal("Unsupported operation CREATE.\n");
    else {
        status = loadobjectdb(dbh);
        if(status != success)
            d_error_fatal("Unable to load object db.");
    }

    switch(command) {
    default:
    case HELP:
        printf("commands: help\n"
               "          create\n"
               "          add <handle> <name> <location> <x> <y> <ax> <ay> <vx> <vy> <onground> <hp> <maxhp> <spname>\n"
               "          remove <name>\n"
               "          view <name>\n"
               "          change <name> <field> <value>\n");
        break;

    case CREATE:
        break;

    case CHANGE:
        status = objectdb_get(dbh, key, &object);
        if(status != success)
            d_error_fatal("Couldn't retreive object.");

        if(strcmp(field, "name") == 0) {
            object.name = value;
        } else if(strcmp(field, "location") == 0) {
            object.location = atoi(value);
        } else if(strcmp(field, "x") == 0) {
            object.x = atoi(value);
        } else if(strcmp(field, "y") == 0) {
            object.y = atoi(value);
        } else if(strcmp(field, "ax") == 0) {
            object.ax = atoi(value);
        } else if(strcmp(field, "ay") == 0) {
            object.ay = atoi(value);
        } else if(strcmp(field, "vx") == 0) {
            object.vx = atoi(value);
        } else if(strcmp(field, "vy") == 0) {
            object.vy = atoi(value);
        } else if(strcmp(field, "onground") == 0) {
            object.onground = (strcmp(argv[i], "false"))?true:false;
        } else if(strcmp(field, "hp") == 0) {
            object.hp = atoi(value);
        } else if(strcmp(field, "maxhp") == 0) {
            object.maxhp = atoi(value);
        } else if(strcmp(field, "spname") == 0) {
            object.spname = value;
        } else
            d_error_fatal("Invalid field ``%s''\n", field);

        status = objectdb_put(dbh, key, &object);
        if(status != success)
            d_error_fatal("Couldn't store object.");
        break;

    case REMOVE:
        status = objectdb_remove(dbh, key);
        if(status != success)
            d_error_fatal("Couldn't remove object.");
        break;

    case ADD:
        status = objectdb_put(dbh, key, &object);
        if(status != success)
            d_error_fatal("Couldn't store object.");
        break;

    case VIEW:
        status = objectdb_get(dbh, key, &object);
        if(status != success)
            d_error_fatal("Couldn't retreive object.");

        printf("%d -> name => %s, location => %d,\n pos => (%d, %d),\n "
               "accel => (%d, %d),\n velocity => (%d, %d),\n "
               "onground => %s,\n hp => (%d/%d)\n spname => %s\n",
               key, object.name, object.location, object.x,
               object.y, object.ax, object.ay, object.vx, object.vy,
               (object.onground == false) ? "false":"true",
               object.hp, object.maxhp, object.spname);
        break;
    }

    closeobjectdb(dbh);
    deletedbh(dbh);
    exit(EXIT_SUCCESS);
}

/* EOF edobject.c */
