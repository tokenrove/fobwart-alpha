/* 
 * edlogin.c ($Id$)
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 *
 * Command-line tool to manage the login database.
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

#define PROGNAME "foblogindb"

int main(int argc, char **argv)
{
    enum { HELP = 0, CREATE, ADD, REMOVE, VIEW, CHANGE } command = HELP;
    loginrec_t loginrec;
    int i, comspec = 0;
    char *key, *field, *value;
    dbhandle_t *dbh;
    bool status;

    d_memory_set(&loginrec, 0, sizeof(loginrec));
    key = field = value = NULL;

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
                    key = argv[i];
                } else if(comspec == 2) {
                    loginrec.password = argv[i];
                } else if(comspec == 3) {
                    loginrec.object = atoi(argv[i]);
                } else
                    d_error_fatal("Too many arguments.\n");
            } else if(command == CHANGE) {
                if(comspec == 1) {
                    key = argv[i];
                } else if(comspec == 2) {
                    field = argv[i];
                } else if(comspec == 3) {
                    value = argv[i];
                } else
                    d_error_fatal("Too many arguments.\n");
            } else if(command == REMOVE ||
                      command == VIEW) {
                if(comspec == 1) {
                    key = argv[i];
                } else
                    d_error_fatal("Too many arguments.\n");
            } else
                d_error_fatal("Bad argument to command. (try ``help'')\n");
            comspec++;
        }
    }

    if(command == CHANGE && (field == NULL || value == NULL))
        command = HELP;

    dbh = newdbh();
    if(command == CREATE) {
        d_error_fatal("CREATE is currently unsupported.\n");
    } else {
        status = loadlogindb(dbh);
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
        status = logindb_get(dbh, key, &loginrec);
        if(status != success)
            d_error_fatal("Failed to retrieve record.\n");

        if(strcmp(field, "password") == 0) {
            loginrec.password = value;
        } else if(strcmp(field, "object") == 0) {
            loginrec.object = atoi(value);
        } else
            d_error_fatal("Invalid field ``%s''\n", field);

        status = logindb_put(dbh, key, &loginrec);
        if(status != success)
            d_error_fatal("Failed to store record.\n");
        break;

    case REMOVE:
        status = logindb_remove(dbh, key);
        if(status != success)
            d_error_fatal("Failed to remove record.\n");
        break;

    case ADD:
        status = logindb_put(dbh, key, &loginrec);
        if(status != success)
            d_error_fatal("Failed to store record.\n");
        break;

    case VIEW:
        status = logindb_get(dbh, key, &loginrec);
        if(status != success)
            d_error_fatal("Failed to retrieve record.\n");

        printf("%s -> %s, %d\n", key, loginrec.password, loginrec.object);
        break;
    }

    closelogindb(dbh);
    deletedbh(dbh);
    exit(EXIT_SUCCESS);
}

/* EOF edlogin.c */
