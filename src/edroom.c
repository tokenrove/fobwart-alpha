/* 
 * edroom.c ($Id$)
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 *
 * Command-line tool for editing the room database.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>

#include "fobwart.h"
#include "fobdb.h"

#define PROGNAME "edroom"


d_set_t *deconsset(char *);
void deconsexits(char *s, roomhandle_t exits[NEXITS_PER_ROOM]);


int main(int argc, char **argv)
{
    enum { HELP = 0, CREATE, ADD, REMOVE, VIEW, CHANGE } command = HELP;
    dword key;
    room_t room;
    int i, comspec = 0;
    char *field, *value;
    dbhandle_t *dbh;
    bool status;
    d_iterator_t it;

    d_memory_set(&room, 0, sizeof(room));
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
                    room.name = argv[i];
                } else if(comspec == 3) {
                    room.gravity = atoi(argv[i]);
                } else if(comspec == 4) {
                    room.islit = (strcmp(argv[i], "false"))?true:false;
                } else if(comspec == 5) {
                    room.mapname = argv[i];
                } else if(comspec == 6) {
                    room.bgname = argv[i];
                } else if(comspec == 7) {
                    room.contents = deconsset(argv[i]);
		} else if(comspec == 8) {
		    deconsexits(argv[i], room.exits);
                } else
                    d_error_fatal("Too many arguments.\n");
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
        status = loadroomdb(dbh);
        if(status != success)
            d_error_fatal("Unable to load room db.");
    }

    switch(command) {
    default:
    case HELP:
        printf("commands: help\n"
               "          create\n"
               "          add <handle> <name> <gravity> <islit> <mapname> <bgname> <contents>\n"
               "          remove <handle>\n"
               "          view <handle>\n"
               "          change <handle> <field> <value>\n");
        break;

    case CREATE:
        break;

    case CHANGE:
        status = roomdb_get(dbh, key, &room);
        if(status != success)
            d_error_fatal("Couldn't retreive room.");

        if(strcmp(field, "name") == 0) {
            room.name = value;
        } else if(strcmp(field, "gravity") == 0) {
            room.gravity = atoi(value);
        } else if(strcmp(field, "islit") == 0) {
            room.islit = (strcmp(argv[i], "false"))?true:false;
        } else if(strcmp(field, "mapname") == 0) {
            room.mapname = value;
        } else if(strcmp(field, "bgname") == 0) {
            room.bgname = value;
        } else if(strcmp(field, "contents") == 0) {
            room.contents = deconsset(value);
	} else if(strcmp(field, "exits") == 0) {
	    deconsexits(value, room.exits);
        } else
            d_error_fatal("Invalid field ``%s''\n", field);

        status = roomdb_put(dbh, key, &room);
        if(status != success)
            d_error_fatal("Couldn't store room.");
        break;

    case REMOVE:
        status = roomdb_remove(dbh, key);
        if(status != success)
            d_error_fatal("Couldn't remove room.");
        break;

    case ADD:
        status = roomdb_put(dbh, key, &room);
        if(status != success)
            d_error_fatal("Couldn't store room.");
        break;

    case VIEW:
        status = roomdb_get(dbh, key, &room);
        if(status != success)
            d_error_fatal("Couldn't retreive room.");

        printf("%ld -> name => %s, gravity => %d,\n islit => %s,\n "
               "mapname => %s, bgname => %s\n contents => ",
               key, room.name, room.gravity,
               (room.islit == false) ? "false":"true",
               room.mapname, room.bgname);
        if(room.contents != NULL) {
            printf("{ ");
            d_iterator_reset(&it);
            while(key = d_set_nextkey(&it, room.contents),
                  key != D_SET_INVALIDKEY) {
                printf("%ld, ", key);
            }
            printf("}\n");
        } else
            printf("{ nullset }\n");
	printf(" exits => { %d, %d, %d, %d }\n", room.exits[0], room.exits[1],
	       room.exits[2], room.exits[3]);
        break;
    }

    closeobjectdb(dbh);
    deletedbh(dbh);
    exit(EXIT_SUCCESS);
}


d_set_t *deconsset(char *s)
{
    char *p;
    dword key;
    d_set_t *set;

    set = d_set_new(0);
    while(p = strtok(s, ","), p != NULL) {
        s = NULL;
        key = atoi(p);
        d_set_add(set, key, NULL);
    }

    return set;
}


void deconsexits(char *s, roomhandle_t exits[NEXITS_PER_ROOM])
{
    char *p;
    dword key;
    int i;

    i = 0;
    while(p = strtok(s, ","), p != NULL) {
        s = NULL;
        key = atoi(p);
	if(i >= NEXITS_PER_ROOM) {
	    fprintf(stderr, "Too many exits!\n");
	}
	exits[i++] = key;
    }

    return;
}

/* EOF edroom.c */
