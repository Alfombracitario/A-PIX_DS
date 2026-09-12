#ifndef FORMATSGLOBALS_H
#define FORMATSGLOBALS_H

#include "nds.h"

//#define DEBUG_CPU
#define DSiMode
#define surfaceMaxExp 7


#define surfaceSize (1<<surfaceMaxExp<<surfaceMaxExp)
#define surfaceBytes (surfaceSize<<1)

#if surfaceMaxExp <= 7
    #define surfaceVramWidth 128
    #define surfaceSizeVRAM (128*128)
    #define surfaceBytesVRAM (128*128*2)
    #define BACKUP_SIZE (surfaceSize * 80)
#else
    #define surfaceVramWidth (1<<surfaceMaxExp)
    #define surfaceSizeVRAM (1<<surfaceMaxExp<<surfaceMaxExp)
    #define surfaceBytesVRAM (2<<surfaceMaxExp<<surfaceMaxExp)
    #define BACKUP_SIZE (surfaceSize * 16)
#endif

#define SURFACE_W 128
#define SURFACE_H 128


extern u16 surface[surfaceSize];
extern u16 backup[BACKUP_SIZE];
extern u16 palette[256];
extern u16 stack[surfaceSize];
extern int paletteSize;
extern u8 paletteBpp;
extern u16 *pixelsTopVRAM;

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
