/* 
 * foblogindb.c
 * Created: Thu Jul 19 17:25:20 2001 by tek@wiw.org
 * Revised: Fri Jul 20 03:42:39 2001 by tek@wiw.org
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
#include "fobserv.h"

#define PROGNAME "foblogindb"

int main(int argc, char **argv)
{
    enum { HELP = 0, CREATE, ADD, REMOVE, VIEW, CHANGE } command = HELP;
    DB *dbp;
    DBT key, data;
    loginrec_t loginrec;
    int i, comspec = 0;
    word wordbuf;
    char *field, *value;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    memset(&loginrec, 0, sizeof(loginrec));
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
                if(key.data == NULL) {
                    key.data = argv[i];
                    key.size = strlen(key.data)+1;
                } else if(loginrec.password == NULL) {
                    loginrec.password = argv[i];
                } else {
                    loginrec.object = atoi(argv[i]);
                }
            } else if(command == CHANGE) {
                if(key.data == NULL) {
                    key.data = argv[i];
                    key.size = strlen(key.data)+1;
                } else if(field == NULL) {
                    field = argv[i];
                } else {
                    value = argv[i];
                }
            } else if(command == REMOVE ||
                      command == VIEW) {
                if(key.data == NULL) {
                    key.data = argv[i];
                    key.size = strlen(key.data)+1;
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
    
    i = dbp->open(dbp, "login.db", NULL, DB_BTREE, DB_CREATE, 0664);
    if(i != 0) {
        dbp->err(dbp, i, "%s", "login.db");
        exit(EXIT_FAILURE);
    }

    switch(command) {
    default:
    case HELP:
        printf("commands: help\n"
               "          create\n"
               "          add <name> <password> <object>\n"
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
        loginrec.password = data.data;
        memcpy(&wordbuf, data.data+strlen(loginrec.password)+1,
               sizeof(wordbuf));
        loginrec.object = ntohs(wordbuf);

        if(strcmp(field, "password") == 0) {
            loginrec.password = value;
        } else if(strcmp(field, "object") == 0) {
            loginrec.object = atoi(value);
        } else {
            fprintf(stderr, "Invalid field ``%s''\n", field);
            exit(EXIT_FAILURE);
        }

        data.size = strlen(loginrec.password)+1+sizeof(loginrec.object);
        data.data = malloc(data.size);
        memcpy(data.data, loginrec.password, strlen(loginrec.password)+1);
        wordbuf = htons(loginrec.object);
        memcpy(data.data+strlen(loginrec.password)+1, &wordbuf,
               sizeof(wordbuf));
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
        data.size = strlen(loginrec.password)+1+sizeof(loginrec.object);
        data.data = malloc(data.size);
        memcpy(data.data, loginrec.password, strlen(loginrec.password)+1);
        wordbuf = htons(loginrec.object);
        memcpy(data.data+strlen(loginrec.password)+1, &wordbuf,
               sizeof(wordbuf));
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
        loginrec.password = data.data;
        memcpy(&wordbuf, data.data+strlen(loginrec.password)+1,
               sizeof(wordbuf));
        loginrec.object = ntohs(wordbuf);
        printf("%s -> %s, %d\n", key.data, loginrec.password,
               loginrec.object);
        break;
    }

    dbp->close(dbp, 0);
    exit(EXIT_SUCCESS);
}

/* EOF foblogindb.c */
