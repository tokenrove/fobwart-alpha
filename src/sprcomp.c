/*
 * Sprite description file compiler/validator
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
    int lineno, mode, i, j, nframes, nanims, framelag;
    int curanim;
    d_image_t *im;
    enum { pcx = 0, tga = 1 } format;

    outfp = NULL; nframes = framelag = nanims = -1;
    lineno = mode = curanim = i = 0;
    im = NULL; format = pcx;

    if(argc != 2) {
        printf("Usage:\nsprcomp <sprite description file>\n");
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
            if(mode != 0)
                bomb("You can't issue the ``%s'' command while defining "
                     "animations. (%d)\n", p, lineno);

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
        } else if(strcmp(p, "nanims") == 0) {
            if(mode != 0)
                bomb("You can't issue the ``%s'' command while defining "
                     "animations. (%d)\n", p, lineno);
            
            p = strtok(NULL, " \t\n");
            if(p == NULL)
                bomb("Looks like you forgot to tell me how many animations "
                     "there are. (%d)\n", lineno);

            if(nanims == -1)
                nanims = atoi(p);
            else
                bomb("Oops, nanims was defined twice. Cowardly exiting now. "
                     "(%d)\n", lineno);
            
            if(nanims < 0)
                bomb("%d is not a sane value for nanims. (%d)\n", nanims,
                     lineno);
            i = atoi(p);
            fputc(i&0xFF, outfp);
        } else if(strcmp(p, "framelag") == 0) {
            if(mode != 0)
                bomb("You can't issue the ``%s'' command while defining "
                     "animations. (%d)\n", p, lineno);
            
            p = strtok(NULL, " \t\n");
            if(p == NULL)
                bomb("Looks like you forgot to tell me the framelag. "
                     "(%d)\n", lineno);
            
            if(framelag == -1)
                framelag = atoi(p);
            else
                bomb("Oops, framelag was defined twice. Cowardly exiting "
                     "now. (%d)\n", lineno);

            if(framelag < 1)
                bomb("%d is not a sane value for framelag. (%d)\n",
                       nanims, lineno);
            i = atoi(p);
            fputc(i&0xFF, outfp);

            /* checksum */
            fputc(0, outfp);
            fputc(0, outfp);
            fputc(0, outfp);
            fputc(0, outfp);
        } else if(strcmp(p, "anim") == 0) {
            if(nanims == -1)
                bomb("It is important to know how many animations there "
                     "are. (You forgot an ``nanims'' line)\n");

            if(i < nframes)
                bomb("You haven't finished specifying the frames for the "
                     "previous animation. (%d)\n", lineno);
            
            mode = 1;
            p = strtok(NULL, " \t\n");
            if(p == NULL)
                bomb("I need to know how many frames there are in this "
                     "animation. (%d)\n", lineno);
            nframes = atoi(p);
            
            if(nframes < 0)
                bomb("%d is not a sane value for nframes. (%d)\n", nframes,
                     lineno);
            fputc(nframes&0xFF, outfp);
            
            i = 0;
        } else if(strcmp(p, "frame") == 0) {
            if(mode != 1)
                bomb("Now is not the time to be defining a frame. (%d)\n",
                     lineno);

            p = strtok(NULL, " \t\n");
            if(p == NULL)
                bomb("I need to know the format of this frame. (%d)\n",
                     lineno);

            if(strcmp(p, "pcx") == 0)
                format = pcx;
            else if(strcmp(p, "tga") == 0)
                format = tga;
            else
                bomb("Never heard of the ``%s'' format. (%d)\n", p, lineno);
            
            p = strtok(NULL, " \t\n");
            if(p == NULL)
                bomb("I need to know the filename of this frame. (%d)\n",
                     lineno);

            switch(format) {
            case pcx:
                im = d_pcx_load(p);
                break;

            case tga:
                im = d_tga_load(p);
                break;
            }

            if(im == NULL)
                bomb("The frame ``%s'' doesn't exist (for me, anyway). "
                     "(%d)\n", p, lineno);

            fputc(im->desc.w&0xff, outfp);
            fputc((im->desc.w>>8)&0xff, outfp);

            fputc(im->desc.h&0xff, outfp);
            fputc((im->desc.h>>8)&0xff, outfp);

            fputc(im->desc.bpp, outfp);
            fputc(im->desc.alpha, outfp);

            j = im->desc.w*im->desc.h*(im->desc.bpp/8);
            fwrite(im->data, 1, j, outfp);
            if(im->desc.alpha > 0 && im->desc.bpp > 8) {
                j = (im->desc.w*im->desc.h*im->desc.alpha+7)/8;
                fwrite(im->alpha, 1, j, outfp);
            }

            i++;
            if(i >= nframes)
                curanim++;

        } else
            bomb("Unrecognized command ``%s'', fleeing. (%d)\n",
                 p, lineno);
    }
    fclose(infp);
    
    if(outfp == NULL)
        bomb("Erg, please name the file something. (Add a ``names'' line)\n");
    fclose(outfp);
    if(curanim < nanims)
        bomb("Insufficient animations defined.\n");
    if(i < nframes)
        bomb("Insufficient frames defined.\n");
    
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
