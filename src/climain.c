/* 
 * climain.c ($Id$)
 * Copyright 2001 Julian E. C. Squires (tek@wiw.org)
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
#include <dentata/random.h>
#include <dentata/util.h>

#include <lua.h>

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"


int gameloop(gamedata_t *gd);
bool clientinit(gamedata_t *gd, char *server, int port);


int main(int argc, char **argv)
{
    gamedata_t gd;
    bool status;
    int mode;
    char *server = "localhost";

    if(argc > 1)
        server = argv[1];

    if(clientinit(&gd, server, FOBPORT) != success)
	return 1;

    /* login */
    status = loginloop(&gd);
    if(status != success)
        return 1;


    /* enter main loop */
    mode = 0;
    while(mode != -1) {
	switch(mode) {
	case 0:
	    mode = gameloop(&gd);
	    break;

	case 1:
	    mode = roomeditloop(&gd);
	    break;

	default:
	    d_error_debug(__FUNCTION__": Unknown mode.\n");
	    mode = -1;
	    break;
	}
    }


    /* close connection */
    net_close(gd.nh);

    /* deinit local */
    evsk_delete(&gd.ws.evsk);
    deinitlocal(&gd);

    /* blow up the outside world */
    destroydata(&gd);
    destroyworldstate(&gd.ws);

    /* dump any extra errors. */
    d_error_dump();

    return 0;
}


/* clientinit
 * Setup the client initial (pre-login) state. */
bool clientinit(gamedata_t *gd, char *server, int port)
{
    bool status;

    /* initialize network */
    status = net_newclient(&gd->nh, server, port);
    if(status == failure) {
        d_error_fatal("Network init failed.\n");
        return failure;
    }

    /* initialize local */
    status = initlocal(gd);
    if(status == failure) {
        d_error_fatal("Local init failed.\n");
        return failure;
    }

    evsk_new(&gd->ws.evsk);

    /* setup a clean world state */
    status = initworldstate(&gd->ws);
    if(status != success) {
        d_error_fatal("Worldstate init failed.\n");
        return failure;
    }

    setluaworldstate(&gd->ws);

    /* load data from server */
    status = loaddata(gd);
    if(status != success) {
        d_error_fatal("Data load failed.\n");
        return failure;
    }

    /* Initialize manager */
    status = d_manager_new();
    if(status == failure)
        return failure;

    d_manager_setscrollparameters(true, 0);

    return success;
}


/* gameloop
 * Performs the main input-update-output loop most commonly thought of
 * as the game. */
int gameloop(gamedata_t *gd)
{
    d_timehandle_t *th;
    widgetstack_t widgets;
    bool focuslockedp;
    widget_t *wp;
    object_t *o;
    int i, retval = -1;

    getobject(gd, gd->localobj);
    if(d_set_fetch(gd->ws.objs, gd->localobj, (void **)&o) != success) {
        d_error_debug("ack. couldn't fetch localobj!\n");
        return -1;
    }
    getroom(gd, o->location);

    /* Reset some miscellaneous variables */
    gd->quitcount = 0;
    gd->status = NULL;

    /* music is disabled atm.
    if(gd->hasaudio && gd->cursong != NULL)
        forkaudiothread(gd->cursong);
    */

    widgets.focus = 0;
    widgets.top = 0;
    widgets.nalloced = 16;
    widgets.stack = d_memory_new(widgets.nalloced*sizeof(widget_t *));
    widgets.stack[widgets.top++] = gamewidget_init(gd);
    widgets.stack[widgets.top++] = msgboxwidget_init(gd);
    widgets.stack[widgets.top++] = ebarwidget_init(gd);
    widgets.stack[widgets.top++] = greetingwidget_init(gd, "READY");
    widgets.stack[widgets.focus]->hasfocus = true;
    focuslockedp = false;

    while(1) {
        th = d_time_startcount(gd->fps, false);
        d_event_update();

	/* Take input from the focused widget */
	wp = widgets.stack[widgets.focus];
	wp->input(wp);

	/* If focus isn't locked, check to see if the user wants a different
	   widget. */
	wp = widgets.stack[widgets.top-1];
	if(!focuslockedp) {
	    widgets.stack[widgets.focus]->hasfocus = false;

	    /* Debugging console */
	    if(d_event_ispressed(EV_CONSOLE) && !isbounce(EV_CONSOLE)) {
		widgets.stack[widgets.top++] = debugconsole_init(gd);
		widgets.focus = widgets.top-1;
		focuslockedp = true;
  
	    /* Inventory */
	    } else if(widgets.focus == 0 &&
		      wp->type != WIDGET_INVENTORY &&
		      d_event_ispressed(EV_INVENTORY) &&
		      !isbounce(EV_INVENTORY)) {


	    /* Quit dialog */
	    } else if(d_event_ispressed(EV_QUIT) && !isbounce(EV_QUIT)) {
		widgets.stack[widgets.top++] = yesnodialog_init(gd,
				          "Are you sure you want to quit?",
							      WIDGET_QUIT);
		widgets.focus = widgets.top-1;
		focuslockedp = true;

	    } else if(d_event_ispressed(EV_ROOMMODE) && !isbounce(EV_ROOMMODE)) {
		retval = 1;
		goto end;

	    /* Cycle focus */
	    } else if(d_event_ispressed(EV_TAB) && !isbounce(EV_TAB)) {
		for(i = widgets.focus+1; i < widgets.top; i++) {
		    if(widgets.stack[i]->takesfocus) {
			widgets.focus = i;
			break;
		    }
		}
		if(i == widgets.top) {
		    for(i = 0; i < widgets.focus; i++) {
			if(widgets.stack[i]->takesfocus) {
			    widgets.focus = i;
			    break;
			}
		    }
		}
	    }

	    widgets.stack[widgets.focus]->hasfocus = true;
	}

	debouncecontrols();

	/* Keep up to date with the server. */
        net_sync(gd);

	/* Update the world. */
        processevents(&gd->ws.evsk, (void *)gd);
        updatephysics(&gd->ws);

	/* Flush the event stack. */
        while(evsk_pop(&gd->ws.evsk, NULL));

	/* Update each widget in order. */
	for(i = 0; i < widgets.top; i++) {
	    wp = widgets.stack[i];
	    /* Update the widget -- is it ready to be closed? */
	    if(wp->update(wp)) {
		d_memory_move(&widgets.stack[i],
			      &widgets.stack[i+1],
			      (widgets.top-i-1)*sizeof(widget_t *));
		widgets.top--;
		if(widgets.focus == i) {
		    widgets.focus = 0;
		    widgets.stack[widgets.focus]->hasfocus = true;
		}

		switch(wp->type) {
		case WIDGET_QUIT:
		    if(yesnodialog_return(wp)) {
			wp->close(wp);
			goto end;
		    }
		    wp->close(wp);
		    focuslockedp = false;
		    break;

		case WIDGET_INVENTORY:
		    /* curitem = inventwidget_return(wp) */
		    wp->close(wp);
		    focuslockedp = false;
		    break;

		case WIDGET_GREETING:
		    wp->close(wp);
		    break;

		case WIDGET_CONSOLE:
		    wp->close(wp);
		    focuslockedp = false;
		    break;
		}
	    }
	}

        d_raster_update();
        d_time_endcount(th);
    }

 end:
    /* wipe widget stack */
    d_memory_delete(widgets.stack);
    return retval;
}


/* EOF climain.c */
