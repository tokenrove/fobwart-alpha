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


bool loaddata(gamedata_t *gd)
{
    bool status;
    packet_t p;
    string_t *requestlist;
    dword csum, nrequests;
    int i;

#if 0
    /* Request checksum list from server. */
    p.type = PACK_GETRESLIST;
    status = net_writepack(gd->nh, p);
    if(status == failure) return failure;
    status = net_readpack(gd->nh, &p);
    if(status == failure || p.type != PACK_RESLIST) return failure;

    nrequests = 0;
    requestlist = NULL;

    checksuminit();

    /* Compute checksums on local resources. */
    for(i = 0; i < p.body.reslist.length; i++) {
	csum = checksumfile(p.body.reslist.res[i].name.data);
	if(csum != p.body.reslist.res[i].checksum) {
	    nrequests++;
	    requestlist = d_memory_resize(requestlist,
					  nrequests*sizeof(string_t));
	    requestlist[nrequests-1] = p.body.reslist.res[i].name;
	}
    }

    /* Request files which don't match or don't exist. */

    /* Free contents of reslist */
#endif

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


/* EOF clidata.c */
