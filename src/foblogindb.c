/* 
 * foblogindb.c
 * Created: Thu Jul 19 17:25:20 2001 by tek@wiw.org
 * Revised: Thu Jul 19 17:47:22 2001 by tek@wiw.org
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * $Id$
 * 
 */

#include <sys/types.h>
#include <limits.h>
#include <db.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define PROGNAME "foblogindb"

int main(int argc, char **argv)
{
    enum { HELP = 0, CREATE, ADD, REMOVE, VIEW, CHANGE } command = HELP;
    DB *dbp;
    int i;

    if(argc > 1)
        command = atoi(argv[1]);

    i = db_open("login.db", DB_BTREE, DB_CREATE, 0664, NULL, NULL, &dbp);
    if(i != 0) {
        fprintf(stderr, "%s: %s\n", PROGNAME, strerror(i));
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
