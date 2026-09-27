#ifndef FORMATS_H
#define FORMATS_H

#include <nds.h>
#include "acs.h"
#include "png/lodepng.h"
#include "gif/gif_lib.h"

int importNES(const char* path, u16* surface);
int exportNES(const char* path, u16* surface, int height);

int importGBC(const char* path, u16* surface);
int exportGBC(const char* path, u16* surface, int height);

int importSNES(const char* path, u16* surface);
int exportSNES(const char* path, u16* surface, int height);

int importGBA(const char* path, u16* surface);
int exportGBA(const char* path, u16* surface, int height);

int importPCX(const char* path, u16* surface, u16* pal);
int exportPCX(const char* path, u16* surface, u16* pal, int width, int height);

int importPal(const char* path, u16* pal);
int exportPal(const char* path, u16* pal);

int importPal1555(const char* path, u16* pal);
int exportPal1555(const char* path, u16* pal);

int importSNES8bpp(const char* path, u16* surface);
int exportSNES8bpp(const char* path, u16* surface, int height);

void saveBMP(const char* filename, uint16_t* pal, uint16_t* surface);
int  loadBMP(const char* filename, uint16_t* pal, uint16_t* surface);
//png
int png_import(const char *path, u16 *surf, u16 *pal);
int png_export(const char *path, const u16 *surf, const u16 *pal);

void importAnim(const char *path);
void exportAnim(const char *path);

int importGIF(const char *filename);
int exportGIF(const char *filename);
//macros
#define formatACS       0
#define formatPNG       1
#define formatGIF       2
#define formatPCX       3
#define formatBMP       4
#define formatNES       5
#define formatGBC       6
#define formatSNES4     7
#define formatSNES8     8
#define formatGBA4      9
#define formatAnim      10
#define formatPAL       11
#define formatPal1555   12
#define formatACSnopal  13
#define formatACSpal    14

#define MaxFormats 15
#define extraSaveFormats 2
const char texts[MaxFormats][16]={
    ".acs",
    ".png",
    ".gif",
    ".pcx",
    ".bmp",
    ".bin[NES]",
    ".bin[GB]",
    ".bin[SNES 4bpp]",
    ".bin[SNES 8bpp]",
    ".bin[GBA 4bpp]",
    ".anim unfished",
    ".pal[YY-CHR]",
    ".pal[1555]",
    ".acs[only img]",
    ".acs[only pal]"
};
const char formats[MaxFormats][6] = {
    ".acs",
    ".png",
    ".gif",
    ".pcx",
    ".bmp",
    ".bin",
    ".bin",
    ".bin",
    ".bin",
    ".bin",
    ".anim",
    ".pal",
    ".pal",
    ".acs",
    ".acs"
};

#define musicFormatWav 0
#define musicFormats 1

const char musFormats[MaxFormats][5] = {
    ".wav",
};


#endif // FORMATS_H