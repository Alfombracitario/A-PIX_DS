#pragma once
#include "formatsglobals.h"
#include "avdslib.h"
#include "textconsole.h"

extern int bucketMode;
extern bool accurate;
extern bool hasClipboard;
extern bool updated;
extern void updatePal(int increment, int *palettePos);
extern void drawColorPalette();
extern void backupWrite();

void copyFromSurfaceToStack();
void cutFromSurfaceToStack();
void pasteFromStackToSurface();
void flipH();
void flipV();
void scaleUp();
void scaleDown();
void rotatePositive();
void rotateNegative();
void shiftDownWrap();
void shiftUpWrap();
void shiftRightWrap();
void shiftLeftWrap();

const char* getFileExtension(const char *path);

// para paletas
inline void copyPalette(){
    int iterations = (paletteBpp >= 8) ? 256 : (1 << paletteBpp);
    for (int i = 0; i < iterations; i++)
    {
        stack[i] = palette[i + paletteOffset];
    }
}
inline void pastePalette(){
    int iterations = (paletteBpp >= 8) ? 256 : (1 << paletteBpp);
    for (int i = 0; i < iterations; i++)
    {
        palette[i + paletteOffset] = stack[i];
    }
}
inline void copyColor(){
    stack[0] = palette[palettePos + paletteOffset];
}
inline void pasteColor(){
    palette[palettePos + paletteOffset] = stack[0];
}
void floodFill(u16 *surface, int x, int y, u16 oldColor, u16 newColor, int xres, int yres);
