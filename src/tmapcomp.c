/*
 * Tilemap description file compiler/validator
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dentata/types.h>
#include <dentata/image.h>
#include <dentata/tga.h>
#include <dentata/pcx.h>

#define BUFLEN 81

void bomb(char *s, ...);

int main(int argc, char **argv)
{
    FILE *infp, *outfp;
    char buffer[BUFLEN], *p;
    int lineno, mode, i, j;
    d_image_t *im, *im2;

    outfp = NULL;
    lineno = i = 0;
    im = im2 = NULL;

    if(argc != 2) {
        printf("Usage:\ntmapcomp <tilemap description file>\n");
        exit(EXIT_FAILURE);
    }
    infp = fopen(argv[1], "r");
    if(infp == NULL) {
        printf("Failed to open %s.\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, BUFLEN-1, infp) != NULL) {
        lineno++;
        if(buffer[strlen(buffer)-1] != '\n')
            bomb("Sorry, but no line should be longer than %d characters, "
                 "even comments. (%d)\n", BUFLEN-1, lineno);
	if(buffer[strlen(buffer)-2] == '\r')
            buffer[strlen(buffer)-2] = '\n';

        p = strtok(buffer, " \t\n");
        if(p == NULL || *p == '#') continue;

        if(strcmp(p, "name") == 0) {
            p = strtok(NULL, " \t\n");
            if(p == NULL)
                bomb("Looks like you forgot to tell me what to name the "
                     "output file. (%d)\n", lineno);
            
            if(outfp == NULL) {
                outfp = fopen(p, "wb");
                if(outfp == NULL)
                    bomb("Failed to open ``%s'' for writing. (%d)\n", p,
                         lineno);
            } else
                bomb("Oops, name was defined twice. Cowardly exiting now. "
                     "(%d)\n", lineno);

        } else if(strcmp(p, "map") == 0) {
            mode = 1;
            p = strtok(NULL, " \t\n");
            if(p == NULL)
                bomb("I need to know the tilemap file. (%d)\n", lineno);
            im = d_pcx_load(p);
            if(im == NULL)
                bomb("%s bad value for map. (%d)\n", p, lineno);
            fputc(im->desc.w, outfp);
            fputc(im->desc.w>>8, outfp);
            fputc(im->desc.h, outfp);
            fputc(im->desc.h>>8, outfp);
            im2 = im;
            i = 0;

        } else if(strcmp(p, "tile") == 0) {
            p = strtok(NULL, " \t\n");
            if(p == NULL)
                bomb("I need to know the name of this tile. (%d)\n",
                     lineno);

            im = d_pcx_load(p);
            if(im == NULL)
                bomb("The tile ``%s'' doesn't exist (for me, anyway). "
                     "(%d)\n", p, lineno);

            if(i == 0) {
                fputc(im->desc.w, outfp);
                fputc(im->desc.w>>8, outfp);
                fputc(im->desc.h, outfp);
                fputc(im->desc.h>>8, outfp);
                fputc(im->desc.bpp, outfp);
                fputc(im->desc.alpha, outfp);
                fputc(0, outfp);
                fputc(0, outfp);
                fputc(0, outfp);
                fputc(0, outfp);
                fwrite(im2->data, 1, im2->desc.w*im2->desc.h, outfp);
            }

            fputc(1, outfp);
            fwrite(im->data, 1, im->desc.w*im->desc.h*(im->desc.bpp/8), outfp);
            if(im->desc.alpha > 0 && im->desc.bpp > 8)
                fwrite(im->alpha, 1,
                       (im->desc.w*im->desc.h*im->desc.alpha+7)/8, outfp);

            i++;
            if(i >= 256)
                bomb("Too (%d) many tiles (%d)\n", i, lineno);

        } else if(strcmp(p, "empty") == 0) {
            if(i == 0)
                bomb("Tile zero must not be empty.\n");

            p = strtok(NULL, " \t\n");
            if(p == NULL)
                bomb("I need to know the tile to fill up to. (%d)\n",
                     lineno);

            j = atoi(p);
            for(; i < j; i++)
                fputc(0, outfp);
            if(i >= 256)
                bomb("Too (%d) many tiles (%d)\n", i, lineno);

        } else
            bomb("Unrecognized command ``%s'', fleeing. (%d)\n",
                 p, lineno);
    }
    fclose(infp);
    
    if(outfp == NULL)
        bomb("Erg, please name the file something. (Add a ``names'' line)\n");
    fclose(outfp);
    
    exit(EXIT_SUCCESS);
}

void bomb(char *s, ...)
{
    va_list ap;

    va_start(ap, s);
    vfprintf(stderr, s, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}
/* EOF sprcomp.c */
