#ifndef ACS_H
#define ACS_H
#include <nds.h>
#include "formatsglobals.h"

void exportACS(const char* path, u16* surface, u16* pal);
void exportACSpal(const char* path, u16* pal);
void exportACSnoPal(const char* path, u16* surface);
void importACS(const char* path, u16* surface, u16* pal);
void importACS16(const char* path, u16* surface, u16* pal);

struct AcsReader
{
    const u8* data;
    size_t    fileSize;
    size_t    headerPos;

    bool hasPalette;
    bool hasAnimation;

    u16 width, height;
    u32 pixelCount;

    u8  bppCode;
    u8  bitsPerPixel;
    u8  colorMode;
    u16 paletteSize;

    u32 ctrlBlockBytes, cmdBlockBytes;
    const u8* ctrlBlock;
    const u8* cmdBlock;
    const u8* pixBlock;
    u32 ctrlBitPos;
    u32 cmdBytePos;
    u32 pixBytePos;

    int outPos;             
    u8  subPixel;
};

extern AcsReader acs;

#endif