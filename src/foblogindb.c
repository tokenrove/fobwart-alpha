/* 
 * foblogindb.c
 * Created: Thu Jul 19 17:25:20 2001 by tek@wiw.org
 * Revised: Thu Jul 19 20:50:07 2001 by tek@wiw.org
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
#include <db3.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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
        } else {
            if(command == ADD) {
                if(key.data == NULL) {
                    key.data = argv[i];
                    key.size = strlen(key.data)+1;
                } else if(loginrec.password == NULL) {
                    loginrec.password = argv[i];
                }
            } else {
                fprintf(stderr, "Bad argument to command. (try ``help'')\n");
                exit(EXIT_FAILURE);
            }
        }
    }

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
    case HELP:
        printf("command: HELP, CREATE, ADD, REMOVE, VIEW, CHANGE\n");
        break;

    case CREATE:
    case ADD:
    case REMOVE:
    case CHANGE:
        break;

    case VIEW:
        break;
    }

    dbp->close(dbp, 0);
    exit(EXIT_SUCCESS);
}

/* EOF foblogindb.c */
