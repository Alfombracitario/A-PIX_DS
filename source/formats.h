#ifndef FORMATS_H
#define FORMATS_H

#include <nds.h>
#include "acs.h"
#include "png/lodepng.h"

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

void saveBMP_indexed(const char* filename, uint16_t* pal, uint16_t* surface);
int  loadBMP_indexed(const char* filename, uint16_t* pal, uint16_t* surface);

void saveBMP(const char* filename, uint16_t* pal, uint16_t* surface);
int  loadBMP_direct(const char* filename, uint16_t* surface);

void saveBMP_4bpp(const char* filename, uint16_t* pal, uint16_t* surface);
int  loadBMP_4bpp(const char* filename, uint16_t* pal, uint16_t* surface);

//png
int png_import(const char *path, u16 *surf, u16 *pal);
int png_export(const char *path, const u16 *surf, const u16 *pal);

//macros
#define formatACS       0
#define formatPNG       1
#define formatPCX       2
#define formatDirectBMP 3
#define format8bppBMP   4
#define format4bppBMP   5
#define formatNES       6
#define formatGBC       7
#define formatSNES4     8
#define formatSNES8     9
#define formatGBA4      10
#define formatPAL       11
#define formatPal1555   12
#define formatACSnopal  13
#define formatACSpal    14

#define MaxFormats 15
#define extraSaveFormats 2
const char texts[MaxFormats][16] = {
    ".acs",
    ".png",
    ".pcx",
    ".bmp[direct]",
    ".bmp[8bpp]",
    ".bmp[4bpp]",
    ".bin[NES]",
    ".bin[GB]",
    ".bin[SNES 4bpp]",
    ".bin[SNES 8bpp]",
    ".bin[GBA 4bpp]",
    ".pal[YY-CHR]",
    ".pal[1555]",
    ".acs[only img]",
    ".acs[only pal]"
};
const char formats[MaxFormats][8] = {
    ".acs",
    ".png",
    ".pcx",
    ".bmp",
    ".bmp",
    ".bmp",
    ".bin",
    ".bin",
    ".bin",
    ".bin",
    ".bin",
    ".pal",
    ".pal",
    ".acs",
    ".acs"
};

#endif // FORMATS_H