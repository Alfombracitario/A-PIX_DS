#ifndef FORMATSGLOBALS_H
#define FORMATSGLOBALS_H

#include "nds.h"

#define surfaceMaxExp 7

#define surfaceSize (1<<surfaceMaxExp<<surfaceMaxExp)
#define surfaceBytes (surfaceSize<<1)
#define BACKUP_SIZE (surfaceSize * 80)

#if surfaceMaxExp <= 7
    #define surfaceVramWidth 128
    #define surfaceSizeVRAM (128*128)
    #define surfaceBytesVRAM (128*128*2)
#else
    #define surfaceVramWidth (1<<surfaceMaxExp)
    #define surfaceSizeVRAM (1<<surfaceMaxExp<<surfaceMax)
    #define surfaceBytesVRAM (2<<surfaceMaxExp<<surfaceMax)
#endif

#define SURFACE_W 128
#define SURFACE_H 128


extern u16 surface[surfaceSize];
extern u16 backup[BACKUP_SIZE];
extern u16 palette[256];
extern u16 stack[surfaceSize];
extern int paletteSize;
extern int paletteBpp;

struct Surface {
    u8 w;    // surface width
    u8 h;    // surface height
    u16 fw;
    u16 fh;
    s16 x;    // surface x offset
    s16 y;    // surface y offset
    s8 z;    // surface zoom
    s8 pz;  // prev zoom
};

extern struct Surface surf;
#endif
