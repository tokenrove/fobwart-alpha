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

#include <sys/types.h>
#include <limits.h>
#include <db.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>

#include <lua.h>

#include "fobwart.h"
#include "fobdb.h"

#define PROGNAME "fobobjectdb"

extern void objencode(object_t *obj, DBT *data);
extern void objdecode(object_t *obj, DBT *data);
extern int bt_handle_cmp(DB *dbp, const DBT *a_, const DBT *b_);


int main(int argc, char **argv)
{
    enum { HELP = 0, CREATE, ADD, REMOVE, VIEW, CHANGE } command = HELP;
    DB *dbp;
    DBT key, data;
    object_t object;
    int i, comspec = 0;
    word wordbuf;
    char *field, *value;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    memset(&object, 0, sizeof(object));
    field = value = NULL;

    for(i = 1; i < argc; i++) {
        if(argv[i][0] == '-') {
        } else if(comspec == 0) {
            if     (strcmp(argv[i], "help") == 0) command = HELP;
            else if(strcmp(argv[i], "create") == 0) command = CREATE;
            else if(strcmp(argv[i], "add") == 0) command = ADD;
            else if(strcmp(argv[i], "remove") == 0) command = REMOVE;
            else if(strcmp(argv[i], "view") == 0) command = VIEW;
            else if(strcmp(argv[i], "change") == 0) command = CHANGE;
            else {
                fprintf(stderr, "Unknown command. (try ``help'')\n");
                exit(EXIT_FAILURE);
            }
            comspec++;
        } else {
            if(command == ADD) {
                if(comspec == 1) {
                    key.size = sizeof(objhandle_t);
                    key.data = malloc(key.size);
                    wordbuf = atoi(argv[i]);
                    memcpy(key.data, &wordbuf, key.size);
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
                }
            } else if(command == CHANGE) {
                if(key.data == NULL) {
                    key.size = sizeof(objhandle_t);
                    key.data = malloc(key.size);
                    wordbuf = atoi(argv[i]);
                    memcpy(key.data, &wordbuf, key.size);
                } else if(field == NULL) {
                    field = argv[i];
                } else {
                    value = argv[i];
                }
            } else if(command == REMOVE ||
                      command == VIEW) {
                if(key.data == NULL) {
                    key.size = sizeof(objhandle_t);
                    key.data = malloc(key.size);
                    wordbuf = atoi(argv[i]);
                    memcpy(key.data, &wordbuf, key.size);
                }
            } else {
                fprintf(stderr, "Bad argument to command. (try ``help'')\n");
                exit(EXIT_FAILURE);
            }
            comspec++;
        }
    }

    if(command == CHANGE && (field == NULL || value == NULL))
        command = HELP;

    i = db_create(&dbp, NULL, 0);
    if(i != 0) {
        fprintf(stderr, "%s: db_create: %s\n", PROGNAME, db_strerror(i));
        exit(EXIT_FAILURE);
    }
    
    i = dbp->set_bt_compare(dbp, bt_handle_cmp);
    if(i != 0) {
        dbp->err(dbp, i, "%s", "set_bt_compare");
        exit(EXIT_FAILURE);
    }

    i = dbp->open(dbp, "object.db", NULL, DB_BTREE, DB_CREATE, 0664);
    if(i != 0) {
        dbp->err(dbp, i, "%s", "object.db");
        exit(EXIT_FAILURE);
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
        i = dbp->get(dbp, NULL, &key, &data, 0);
        if(i != 0) {
            dbp->err(dbp, i, "dbp->get");
            exit(EXIT_FAILURE);
        }
        objdecode(&object, &data);

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
        } else {
            fprintf(stderr, "Invalid field ``%s''\n", field);
            exit(EXIT_FAILURE);
        }

        objencode(&object, &data);
        i = dbp->put(dbp, NULL, &key, &data, 0);
        if(i != 0) {
            dbp->err(dbp, i, "dbp->put");
            exit(EXIT_FAILURE);
        }
        break;

    case REMOVE:
        i = dbp->del(dbp, NULL, &key, 0);
        if(i != 0) {
            dbp->err(dbp, i, "dbp->del");
            exit(EXIT_FAILURE);
        }
        break;

    case ADD:
        objencode(&object, &data);
        i = dbp->put(dbp, NULL, &key, &data, DB_NOOVERWRITE);
        if(i != 0) {
            dbp->err(dbp, i, "dbp->put");
            exit(EXIT_FAILURE);
        }
        break;

    case VIEW:
        i = dbp->get(dbp, NULL, &key, &data, 0);
        if(i != 0) {
            dbp->err(dbp, i, "dbp->get");
            exit(EXIT_FAILURE);
        }
        objdecode(&object, &data);
        printf("%d -> name => %s, location => %d,\n pos => (%d, %d),\n accel => (%d, %d),\n velocity => (%d, %d),\n onground => %s,\n hp => (%d/%d)\n spname => %s\n", *((objhandle_t *)key.data), object.name, object.location, object.x,
               object.y, object.ax, object.ay, object.vx, object.vy,
               (object.onground == false) ? "false":"true",
               object.hp, object.maxhp, object.spname);
        break;
    }

    dbp->close(dbp, 0);
    exit(EXIT_SUCCESS);
}

/* EOF edobject.c */
