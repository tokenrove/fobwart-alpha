/*
 * clidata.c
 * Julian Squires <tek@wiw.org> / 2001
 */


#include "fobwart.h"
#include "fobnet.h"
#include "fobcli.h"

#include <dentata/memory.h>
#include <dentata/color.h>
#include <dentata/manager.h>
#include <dentata/error.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>


bool loaddata(gamedata_t *gd);
void destroydata(gamedata_t *gd);
static bool savefile(char *name, byte *data, dword len);


bool loaddata(gamedata_t *gd)
{
    bool status;
    packet_t p, p2;
    dword csum;
    int i;

    /* Request checksum list from server. */
    p.type = PACK_GETRESLIST;
    status = net_writepack(gd->nh, p);
    if(status == failure) return failure;

    status = net_readpack(gd->nh, &p);
    if(status == failure || p.type != PACK_RESLIST) return failure;

    checksuminit();

    /* Compute checksums on local resources. */
    for(i = 0; i < p.body.reslist.length; i++) {
	csum = checksumfile((char *)p.body.reslist.name[i].data);

	/* Request files which don't match or don't exist. */
	if(csum != p.body.reslist.checksum[i]) {
	    p2.type = PACK_GETFILE;
	    p2.body.string = p.body.reslist.name[i];
	    status = net_writepack(gd->nh, p2);
	    if(status == failure) return failure;

	    fprintf(stderr, __FUNCTION__": readpack\n");
	    status = net_readpack(gd->nh, &p2);
	    if(status == failure || p2.type != PACK_FILE) return failure;

	    fprintf(stderr, __FUNCTION__": downloaded %s (%ld, %lx) from "
		    "server.\n", p2.body.file.name.data, p2.body.file.length,
		    p2.body.file.checksum);

	    if(savefile((char *)p2.body.file.name.data, p2.body.file.data,
			p2.body.file.length) != success)
		return failure;

	    csum = checksumfile((char *)p2.body.file.name.data);
	    if(csum != p2.body.file.checksum) {
		d_error_debug(__FUNCTION__": checksum failed on %s! "
			      "(%lx->%lx)\n", p2.body.file.name.data,
			      p2.body.file.checksum, csum);
		return failure;
	    }

	    d_memory_delete(p2.body.file.data);
	    d_memory_delete(p2.body.file.name.data);
	}
    }

    /* Free contents of reslist */
    reslist_delete(&p.body.reslist);

    /* load fonts */
    gd->deffont = d_font_load(DEFFONTFNAME);
    if(gd->deffont == NULL) return failure;
    gd->largefont = d_font_load(LARGEFONTFNAME);
    if(gd->largefont == NULL) return failure;

    /* Load palettes */
    loadpalette(LIGHTPALFNAME, &gd->light);
    loadpalette(DARKPALFNAME, &gd->dark);
    d_memory_copy(&gd->raster->palette, &gd->light,
                  D_NCLUTITEMS*D_BYTESPERCOLOR);

    /* Munge fonts into most acceptable forms */
    d_font_convertdepth(gd->deffont, gd->raster->desc.bpp);
    d_font_convertdepth(gd->largefont, gd->raster->desc.bpp);

    d_font_silhouette(gd->largefont,
                      d_color_fromrgb(gd->raster, 255, 255, 255),
                      255);
    d_font_silhouette(gd->deffont, d_color_fromrgb(gd->raster, 0, 0, 15),
                      255);


    /* Create our energy bar. */
    gd->ebar = ebar_new(gd->raster);

    /* Load audio
       if(gd->hasaudio) {
       gd->cursong = d_s3m_load(DATADIR "/mm2.s3m");
       }
    */

    return success;
}


/* destroydata
 * Welcome to memory leaks. */
void destroydata(gamedata_t *gd)
{
    /* Free current song
       if(gd->hasaudio)
       d_s3m_delete(gd->cursong);
    */

    d_image_delete(gd->ebar);
    d_manager_delete();

    /* Destroy fonts */
    d_font_delete(gd->largefont);
    gd->largefont = NULL;
    d_font_delete(gd->deffont);
    gd->deffont = NULL;
    return;
}


bool savefile(char *name, byte *data, dword len)
{
    FILE *fp;

    fp = fopen(name, "w");
    if(!fp) {
	fprintf(stderr, "Failed to open %s for writing: %s\n", name,
		strerror(errno));
	return failure;
    }

    if(fwrite(data, 1, len, fp) != len) {
	fprintf(stderr, "Failed to write %ld bytes into %s.\n", len,
		name);
	return failure;
    }

    fclose(fp);

    return success;
}


/* EOF clidata.c */
