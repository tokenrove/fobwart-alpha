/*
 * rmedmode.c
 * Julian Squires <tek@wiw.org> / 2001
 */

#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"

#include <dentata/time.h>
#include <dentata/memory.h>
#include <dentata/event.h>
#include <dentata/raster.h>

int roomeditloop(gamedata_t *gd);

/* roomeditloop
 * The online room editor. */
int roomeditloop(gamedata_t *gd)
{
    d_timehandle_t *th;
    widgetstack_t widgets;
    bool focuslockedp;
    widget_t *wp;
    int i, retval = -1;

    widgets.top = 0;
    widgets.nalloced = 16;
    widgets.stack = d_memory_new(widgets.nalloced*sizeof(widget_t *));
    widgets.stack[widgets.top++] = mapeditwidget_init(gd);
    widgets.focus = 0;
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
	    /* Quit dialog */
	    if(d_event_ispressed(EV_QUIT) && !isbounce(EV_QUIT)) {
		widgets.stack[widgets.top++] = yesnodialog_init(gd,
				          "Are you sure you want to quit?",
							      WIDGET_QUIT);
		widgets.focus = widgets.top-1;
		focuslockedp = true;

	    /* Move to game mode */
	    } else if(d_event_ispressed(EV_GAMEMODE) && !isbounce(EV_GAMEMODE)) {
		/* FIXME -- if room is changed, ask if user is sure. */
		retval = 0;
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

/* rmedmode.c */
