/*
    ADVERTENCIA: este será el código con más bitshifts y comentarios inecesarios que verás, suerte tratando de entender algo!
     -Alfombracitario, Septiembre de 2025
*/

/*
    To-Do list (para v1.0):
    reordenamiento de código (en proceso)
    añadir figuras (dos pasos)
        rectangulo
        circulo
        línea

    select tool
    move
*/

#include <nds.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>

#include <font.h>

#include <filesystem.h>
#include "timers.h"
#include "textconsole.h"
#include "files.h"
#include "avdslib.h"
#include "intro.h" //intro global para todos mis juegos
#include "animation.h"
#include "formatsglobals.h"
#include "music.h"
#include "tools.h"
#include "acs.h"

#include "GFXinput.h"
#include "GFXconsoleInput.h"
#include "GFXnewImageInput.h"

// Macros
#define SCREEN_W 256
#define SCREEN_H 192
#define SURFACE_X 64
#define SURFACE_Y 0

#define C_WHITE 65535
#define C_RED 32799
#define C_YELLOW 33791
#define C_GREEN 33760
#define C_CYAN 65504
#define C_BLUE 64512
#define C_PURPLE 64543
#define C_BLACK 32768
#define C_GRAY 48623

#define STYLUSHOLDTIME 15

#define MAX_ALPHA 63

// OAM
#define selector24oamID 64
#define brushSettingsOamId 24
#define brushSettingsSelectorOamId 22
#define selector16oamId 64
#define rgbSliderOamId 66
#define paletteOamId 26
#define paletteSelOamId 25
#define rgbSliderSelOamId 65
#define selectedZoneOamId 100
#define isGridOamId 101
#define isAudioSyncOamId 102
#define isClipboardOamId 103
#define isOnionSkinOamId 104

#define gridOamId 0

#define rgbSliderX SURFACE_X + SURFACE_W
#define rgbSliderY 32

//===================================================================Variables================================================================
static PrintConsole topConsole;
static PrintConsole subConsole;

u16 surface[surfaceSize];//lienzo principal, si pudiera lo metería a dtcm
u16 *pixelsTopVRAM = (u16 *)BG_GFX;
u16 *pixelsVRAM = (u16 *)BG_GFX_SUB;
u16 *bgPreviewGfx = NULL;
u16 pixelsTop[surfaceSize];// surface procesado en RAM.
u16 DTCM_DATA palette[256]; // ram rápida sin cache miss, perfecto para acceso aleatorio de paletas

u16 stack[surfaceSize]; // para operaciones temporales
u16 backup[BACKUP_SIZE];
u16 onionSkin[surfaceSize];

u16 gradientTable[SCREEN_H];

u16 *gfx32;
u16 *gfx16;
u16 *gfxBG;
u16 *gfxRGBsliders;
u16 *gfxRgbSliderSel;
u16 *gfxPalette;
u16 *gfx5;
u16 *gfxGrid;
u16 *gfxSelectedZone;
u8 paletteAlpha = MAX_ALPHA; // indicador del alpha actual, útil para 16bpp
// ideal añadir un array para guardar más frames

touchPosition touch;

int bgCanvas;
int bgUI;

int backupSize = surfaceBytes;
int backupMax = BACKUP_SIZE/surfaceBytes;

int backupIndex = -1; // índice del último frame guardado
int oldestBackup = 0; // límite inferior (el frame más antiguo que aún es válido)
int totalBackups = 0; // cuántos backups se han llenado realmente

// paletas
int paletteSize = 256;
int DTCM_DATA palettePos = 0;
int DTCM_DATA paletteOffset = 0;
u8 DTCM_DATA paletteBpp = 8;


int bucketMode;
bool onionSkinEnable = false;
bool hasClipboard = false;
bool nesMode = false;
bool usesPages = false;
bool moveCanvas = false;
bool repeatCanvas = false;
bool imgChanges = false;

u8 DTCM_DATA palEdit[3];

const u16 nesPalette[64] = {
    0xbdef, 0xd804, 0xdc05, 0xd04c, 0xbc93, 0x9856, 0x80d4, 0x810f,
    0x8169, 0x81a7, 0x81a7, 0xa186, 0xc146, 0x8000, 0x8000, 0x8000,
    0xdef7, 0xfd88, 0xfd08, 0xf912, 0xe11b, 0xb11b, 0x815c, 0x81d8,
    0x8231, 0x828a, 0x8aa9, 0xb689, 0xe248, 0x8000, 0x8000, 0x8000,
    0xffff, 0xfe8c, 0xfe0a, 0xfdd4, 0xfd9e, 0xd99f, 0x99ff, 0x829f,
    0x935d, 0x83b3, 0xa3ce, 0xcb8e, 0xf34c, 0xb18c, 0x8000, 0x8000,
    0xffff, 0xff52, 0xfef4, 0xfed8, 0xfedc, 0xf6ff, 0xdf3f, 0xd37f,
    0xcbdf, 0xc3d9, 0xd3d4, 0xe7f4, 0xfbf4, 0xd294, 0x8000, 0x8000};

// otras variables

DTCM_DATA struct Surface surf = {
    .w = surfaceMaxExp,
    .h = surfaceMaxExp,
    .fw = 1<<surfaceMaxExp,
    .fh = 1<<surfaceMaxExp,
    .x = 0,
    .y = 0,
    .z = 2,
    .pz = 2
};

u16 DTCM_DATA prevtpx = 0;
u16 DTCM_DATA prevtpy = 0;

int previewXoffset = 0;
int previewYoffset = 0;
int previewPosAlpha = 15;

int prevx = 0;
int prevy = 0;

int imgFormat = 0;

int stylusHoldTimer = STYLUSHOLDTIME;
bool stylusRepeat = false;

int stackYres = surfaceMaxExp;
int stackXres = surfaceMaxExp;

// usado como variable temporal en la creación de imagen nueva
int resX = 7;
int resY = 7;

u8 palEditSel = 1;

u32 DTCM_DATA kDown = 0;
u32 DTCM_DATA kHeld = 0;
u32 DTCM_DATA kUp = 0;

u32 DTCM_DATA frameStartTime = 0;
u32 DTCM_DATA frameEndTime = 0;

bool stylusPressed = false;
bool showGrid = false;
bool drew = false;
bool updated = false;
bool accurate = false;
bool mayus = false;
int holdTimer = 0;
int fileOffset = 0;
int gridSkips = 0;
bool rPressed = false;
bool showBrushSettings = false;
bool preview = true;
bool redraw = true;

u32 effectBackupPos = 0;

// Variables globales para controlar el modo actual
subMode currentSubMode = SUB_BITMAP;

consoleMode currentConsoleMode = MODE_NO;

enum{
    ACTION_NONE = 0,
    ACTION_UP = 1 << 0,
    ACTION_DOWN = 1 << 1,
    ACTION_LEFT = 1 << 2,
    ACTION_RIGHT = 1 << 3,
    ACTION_ZOOM_IN = 1 << 4,
    ACTION_ZOOM_OUT = 1 << 5,
};

typedef enum
{
    TOOL_BRUSH,
    TOOL_ERASER,
    TOOL_BUCKET,
    TOOL_PICKER
} ToolType;

ToolType currentTool = TOOL_BRUSH; // por defecto

typedef enum
{
    BRUSH_MODE_NORMAL = 0,
    BRUSH_MODE_DITHER,
    BRUSH_MODE_VLINES,
    BRUSH_MODE_HLINES,
} BrushMode;

typedef enum
{
    BRUSH_SIZE_1 = 0,
    BRUSH_SIZE_2,
    BRUSH_SIZE_3,
    BRUSH_SIZE_4
} BrushSize;

typedef enum
{
    BRUSH_TYPE_RECTANGLE_HOLLOW = 0,
    BRUSH_TYPE_RECTANGLE_FILLED,
    BRUSH_TYPE_LINE,
    BRUSH_TYPE_CIRCLE
} BrushType;

BrushMode brushMode = BRUSH_MODE_NORMAL;
BrushSize brushSize = BRUSH_SIZE_1;

ConsoleFont font = {
    .gfx = fontTiles,
    .pal = fontPal,
    .numColors = fontPalLen >> 1,
    .bpp = 4,
    .asciiOffset = 32,
    .numChars = fontTilesLen >> 5,
};

#define RM 31
#define GM (31<<5)
#define BM (31<<10)

u16 mergeColor(u16 o, u16 n){
    const u32 s = (u32)n << 1;

    const u32 r =
    ((s & (RM*2))+(o & RM)+(n & RM))>>2;
    const u32 g =
    (((s & (GM*2))+(o & GM)+(n & GM))>>2) & GM;
    const u32 b =
    (((s & (BM*2))+(o & BM)+(n & BM))>>2) & BM;

    return (u16)(r|g|b|0x8000);
}

// FUNCIONES
void submitVRAM(bool _accurate = false, bool _wait = true)
{

    if (paletteBpp != 16)
    {
        if (_accurate)
            DC_FlushRange(pixelsTop, surfaceBytesVRAM);

        DMA2_CR = 0;
        DMA2_SRC  = (u32)pixelsTop;
        DMA2_DEST = (u32)pixelsTopVRAM;
        DMA2_CR   = surfaceSizeVRAM | DMA_ENABLE;
        
        // leer desde pixelsTop, no desde VRAM
        DMA3_CR = 0;
        DMA3_SRC  = (u32)pixelsTop;
        DMA3_DEST = (u32)pixelsVRAM;
        DMA3_CR   = surfaceSizeVRAM | DMA_ENABLE;

        if(_wait){
            while (DMA2_CR & DMA_ENABLE);
            while (DMA3_CR & DMA_ENABLE);
        }
    }
    else
    {
        if (_accurate)
            DC_FlushRange(pixelsTop, surfaceBytesVRAM);
        //pasamos de VRAM a sub
        DMA3_CR = 0;
        DMA3_SRC  = (u32)pixelsTopVRAM;
        DMA3_DEST = (u32)pixelsVRAM;
        DMA3_CR   = surfaceSizeVRAM | DMA_ENABLE;
        if(_wait)
            while (DMA3_CR & DMA_ENABLE);
    }
}
ITCM_CODE static void vblank_handler(void)
{
    // Stop the previous DMA copy
    dmaStopSafe(0);

    BG_PALETTE[0] = gradientTable[0];

    dmaSetParams(0,
                 &gradientTable[1],
                 &BG_PALETTE[0],              // Write to the background color
                 DMA_SRC_INC |                // Autoincrement source after each copy
                     DMA_DST_FIX |            // Keep destination fixed
                     DMA_START_HBL |          // Start copy at the start of horizontal blank
                     DMA_REPEAT |             // Don't stop DMA after the first copy.
                     DMA_COPY_HALFWORDS | 1 | // Copy one halfword each time
                     DMA_ENABLE);
}

void initGradient()
{
    for (int i = 0; i < 32; i++)
    {
        int r = 15 - i;
        if (r < 0)
            r = 0;

        int b = (31 - i) >> 1;
        u16 color = (b << 10) | (r<<isGreen);
        gradientTable[i] = color;
        gradientTable[SCREEN_H - i] = color;
    }
    //pequeña probabilidad de que el gradiente se invierta de colores :>
    irqSet(IRQ_VBLANK, vblank_handler); // configurar HDMA
}
ITCM_CODE bool brushPatternPass(int x, int y, BrushMode mode)
{
    switch (mode)
    {
    case BRUSH_MODE_NORMAL:
        return true;

    case BRUSH_MODE_HLINES:
        return !(y & 1);

    case BRUSH_MODE_VLINES:
        return !(x & 1);

    case BRUSH_MODE_DITHER:
        return ((x ^ y) & 1) == 0;
    }

    return true;
}
ITCM_CODE void drawPixelSurface(int x, int y, u16 color)
{
    if ((unsigned)x < surf.fw &&
        (unsigned)y < surf.fh &&
        brushPatternPass(x, y, brushMode))
    {
        surface[(y << surf.w) + x] = color;
    }
}
u16 mergeColorAlpha(u16 oldCol, u16 color, u8 alpha)
{
    if (alpha > MAX_ALPHA)
        alpha = MAX_ALPHA;

    u8 r1 = (oldCol >> 10) & 31;
    u8 g1 = (oldCol >> 5) & 31;
    u8 b1 = oldCol & 31;

    u8 r = (color >> 10) & 31;
    u8 g = (color >> 5) & 31;
    u8 b = color & 31;

    int r2 = (r * alpha + r1 * (MAX_ALPHA - alpha)) / MAX_ALPHA;
    int g2 = (g * alpha + g1 * (MAX_ALPHA - alpha)) / MAX_ALPHA;
    int b2 = (b * alpha + b1 * (MAX_ALPHA - alpha)) / MAX_ALPHA;

    if (alpha > 0)
    {
        if (r2 == r1)
            r2 += (r > r1) - (r < r1);
        if (g2 == g1)
            g2 += (g > g1) - (g < g1);
        if (b2 == b1)
            b2 += (b > b1) - (b < b1);
    }

    return (r2 << 10) | (g2 << 5) | b2 | 0x8000;
}

ITCM_CODE void drawPixelSurfaceAlpha(int x, int y, u16 color)
{
    if ((unsigned)x < surf.fw &&
        (unsigned)y < surf.fh &&
        brushPatternPass(x, y, brushMode))
    {
        int index = (y << surf.w) + x;
        surface[index] = mergeColorAlpha(surface[index], color, paletteAlpha);
    }
}

inline void brushStamp2(int x, int y, u16 color)
{
    if (paletteAlpha != MAX_ALPHA)
    {
        drawPixelSurfaceAlpha(x, y, color);
        drawPixelSurfaceAlpha(x + 1, y, color);
        drawPixelSurfaceAlpha(x, y + 1, color);
        drawPixelSurfaceAlpha(x + 1, y + 1, color);
    }
    else
    {
        drawPixelSurface(x, y, color);
        drawPixelSurface(x + 1, y, color);
        drawPixelSurface(x, y + 1, color);
        drawPixelSurface(x + 1, y + 1, color);
    }
}

inline void brushStamp3Square(int x, int y, u16 color)
{
    if (paletteAlpha != MAX_ALPHA)
    {
        for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++)
                drawPixelSurfaceAlpha(x + ox, y + oy, color);
    }
    else
    {
        for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++)
                drawPixelSurface(x + ox, y + oy, color);
    }
}

inline void brushStamp3Circle(int x, int y, u16 color)
{
    if (paletteAlpha != MAX_ALPHA)
    {
        drawPixelSurfaceAlpha(x, y, color);
        drawPixelSurfaceAlpha(x + 1, y, color);
        drawPixelSurfaceAlpha(x, y + 1, color);
        drawPixelSurfaceAlpha(x - 1, y, color);
        drawPixelSurfaceAlpha(x, y - 1, color);
    }
    else
    {
        drawPixelSurface(x, y, color);
        drawPixelSurface(x + 1, y, color);
        drawPixelSurface(x, y + 1, color);
        drawPixelSurface(x - 1, y, color);
        drawPixelSurface(x, y - 1, color);
    }
}
inline void brushStamp4Circle(int x, int y, u16 color)
{
    if (paletteAlpha != MAX_ALPHA)
    {
        drawPixelSurfaceAlpha(x + 1, y - 1, color);
        drawPixelSurfaceAlpha(x, y - 1, color);
        drawPixelSurfaceAlpha(x - 1, y + 1, color);
        drawPixelSurfaceAlpha(x - 1, y, color);
        drawPixelSurfaceAlpha(x, y, color);
        drawPixelSurfaceAlpha(x, y + 1, color);
        drawPixelSurfaceAlpha(x, y + 2, color);
        drawPixelSurfaceAlpha(x + 1, y + 1, color);
        drawPixelSurfaceAlpha(x + 1, y + 2, color);
        drawPixelSurfaceAlpha(x + 2, y + 1, color);
        drawPixelSurfaceAlpha(x + 1, y, color);
        drawPixelSurfaceAlpha(x + 2, y, color);
    }
    else
    {
        drawPixelSurface(x + 1, y - 1, color);
        drawPixelSurface(x, y - 1, color);
        drawPixelSurface(x - 1, y + 1, color);
        drawPixelSurface(x - 1, y, color);
        drawPixelSurface(x, y, color);
        drawPixelSurface(x, y + 1, color);
        drawPixelSurface(x, y + 2, color);
        drawPixelSurface(x + 1, y + 1, color);
        drawPixelSurface(x + 1, y + 2, color);
        drawPixelSurface(x + 2, y + 1, color);
        drawPixelSurface(x + 1, y, color);
        drawPixelSurface(x + 2, y, color);
    }
}

inline void brushStamp4Square(int x, int y, u16 color)
{
    if (paletteAlpha != MAX_ALPHA)
    {
        for (int oy = -1; oy <= 2; oy++)
            for (int ox = -1; ox <= 2; ox++)
                drawPixelSurfaceAlpha(x + ox, y + oy, color);
    }
    else
    {
        for (int oy = -1; oy <= 2; oy++)
            for (int ox = -1; ox <= 2; ox++)
                drawPixelSurface(x + ox, y + oy, color);
    }
}

// decidir el patrón y tamaño de pincel
inline void brushStamp(int x, int y, u16 color)
{
    bool forceSquare = (brushMode == BRUSH_MODE_DITHER);

    switch (brushSize)
    {
    case BRUSH_SIZE_1:
        if (paletteAlpha != MAX_ALPHA)
        {
            drawPixelSurfaceAlpha(x, y, color);
        }
        else
        {
            drawPixelSurface(x, y, color);
        }
        break;

    case BRUSH_SIZE_2:
        brushStamp2(x, y, color);
        break;

    case BRUSH_SIZE_3:
        if (forceSquare)
            brushStamp3Square(x, y, color);
        else
            brushStamp3Circle(x, y, color);
        break;

    case BRUSH_SIZE_4:
        if (forceSquare)
            brushStamp4Square(x, y, color);
        else
            brushStamp4Circle(x, y, color);
        break;
    }
}

ITCM_CODE void drawLineSurface(int x0, int y0, int x1, int y1, u16 color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1)
    {
        brushStamp(x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

ITCM_CODE void drawLineSurfaceAlpha(int x0, int y0, int x1, int y1, u16 color)
{
    const int dx = abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1)
    {
        brushStamp(x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

ITCM_CODE void drawGrid(u16 color) {
    int separation = 1 << (surf.z + gridSkips);

    dmaFillWords(0, gfxGrid, 64 * 64 * 2);

    if (separation < 2 || separation > 64)
        return;

    for (int i = gridOamId; i < gridOamId + 4; i++)
        oamSetHidden(&oamSub, i, false);

    int phaseX = (surf.x << surf.z) & (separation - 1);
    int phaseY = (surf.y << surf.z) & (separation - 1);

    // líneas verticales
    for (int x = -phaseX; x < 64; x += separation) {
        if (x >= 0) {
            u16* p = gfxGrid + x;
            for (int j = 0; j < 64; j++, p += 64)
                *p = color;
        }
    }

    // líneas horizontales
    for (int y = -phaseY; y < 64; y += separation) {
        if (y >= 0)
            dmaFillWords((u32)(color<<16)|color, gfxGrid + y * 64, 64 * 2);
    }
}

void updatePreviewPos(){
    //este sprite mide 64x64
    //obtenemos los datos como el offset y otras cosas
    oamSet(&oamMain, selectedZoneOamId,
        previewXoffset+surf.x,
        previewYoffset+surf.y,
        0, previewPosAlpha,
        SpriteSize_64x64, SpriteColorFormat_Bmp,
        gfxSelectedZone,
        -1,
        false, false, false, false, false);
    oamUpdate(&oamMain);
    
}

ITCM_CODE void updatePreviewGfx(){
    //reiniciamos visualmente todo
    u16 color = AVinvertColor(palette[paletteOffset]);
    dmaFillWords(0,gfxSelectedZone, 64 * 64 * 2);

    if(surf.z > 0){
        const int _size = 1<<(7-surf.z);

        const int offset = ((_size - 1) << 6);
        const int size2 = _size-1;

        // línea superior e inferior
        for (int i = 0; i < _size; i++) {
            gfxSelectedZone[i] = color;// top
        }
        for (int i = offset; i < _size+offset; i++) {
            gfxSelectedZone[i] = color;// bottom
        }
        
        // líneas laterales
        for (int j = 1; j < (_size - 1); j++){
            gfxSelectedZone[(j << 6)]         = color;// left
            gfxSelectedZone[(j << 6) + size2] = color;// right
        }
    }
}
//=========================================================DRAW SURFACE========================================================================
ITCM_CODE void drawSurfaceMainOnionSkin()
{
    updated = true;
    const int size = surf.fw<<surf.h;
    const u16 bgCol = palette[0];
    if (paletteBpp == 16)
    {
        u16 *dst = (u16*)pixelsTopVRAM; // directo a VRAM
        const u16 *src = surface;
        if (surf.fw == surfaceVramWidth)
        {
            for(int i = 0; i < size; i++){
                if(onionSkin[i] == bgCol){
                    dst[i] = src[i];
                }else{
                    dst[i] = mergeColor(onionSkin[i],src[i]);
                }
            }
            return;
        }
        int i = 0;
        for(int y = 0; y < surf.fh; y++){
            i = y<<7;//tamaño de la surface
            for(int x = 0; x < surf.fw; x++){
                if(onionSkin[i] == bgCol){
                    dst[i] = src[i];
                }else{
                    dst[i] = mergeColor(onionSkin[i],src[i]);
                }
                i++;
            }
        }
        return;
    }

    const u16 *pal = palette + paletteOffset;
    const u16 *src = surface;
    u16 *dst = pixelsTop;

    int i = 0;
    for(int y = 0; y < surf.fh; y++){
        i = y<<7;//tamaño de la surface
        for(int x = 0; x < surf.fw; x++){
            dst[i] = onionSkin[i] != bgCol ? mergeColor(onionSkin[i],pal[src[i]]) : pal[src[i]];
            i++;
        }
    }
}
ITCM_CODE void drawSurfaceMain()
{
    if(onionSkinEnable){
        drawSurfaceMainOnionSkin();
        return;
    }
    updated = true;

    if (paletteBpp == 16)
    {
        u16 *dst = (u16*)pixelsTopVRAM; // directo a VRAM
        const u16 *src = surface;
        if (surf.fw == surfaceVramWidth)
        {
            memcpy(dst,src,surfaceVramWidth<<surfaceMaxExp<<1);
            return;
        }
        for (int i = 0; i < surf.fh; i++, dst += surfaceVramWidth, src += surf.fw)
        {
            u32 *dst32 = (u32*)dst;
            const u32 *src32 = (const u32*)src;
            for (int j = 0; j < (surf.fw >> 1); j++)
                dst32[j] = src32[j];
        }
        return;
    }

    // modo paleta — sin cambios
    const u16 *pal = palette + paletteOffset;
    const u16 *src = surface;
    u16 *dst = pixelsTop;

    for (int i = 0; i < surf.fh; i++, dst += surfaceVramWidth, src += surf.fw)
    {
        const u16 *row = src;
        u32 *dst32 = (u32*)dst;
        int j = 0;
        for (; j < surf.fw - 1; j += 2)
        {
            u32 a = pal[row[j]];
            u32 b = pal[row[j + 1]];
            dst32[j >> 1] = a | (b << 16);
        }
        if (j < surf.fw)
            dst[j] = pal[row[j]];
    }
}

void drawSurfaceBottom()
{ // esta funcion ahora se encarga de limitar y actualizar ciertos datos.
    int visibleX = 128 >> surf.z;
    int visibleY = 128 >> surf.z;

    int maxX = (surf.fw) - visibleX;
    int maxY = (surf.fh) - visibleY;

    if (maxX < 0) maxX = 0;
    if (maxY < 0) maxY = 0;
    if (surf.x < 0)
        surf.x = 0;
    if (surf.y < 0)
        surf.y = 0;
    if (surf.x > maxX)
        surf.x = maxX;
    if (surf.y > maxY)
        surf.y = maxY;
    
    // --- Limitar el zoom ---
    if(surf.pz != surf.z){
        int maxRes = MAX(surf.w, surf.h);
    
        int minZoom = surfaceMaxExp - maxRes;
        int maxZoom = 6;
        if (surf.z < minZoom)
        {
            surf.z = minZoom;
        }
        if (surf.z > maxZoom){
            surf.z = maxZoom;
        }
    }


    s16 scale = 256 >> surf.z;
    //un lut para los zooms
    s16 oamScale = 1<<(surf.z+4);
    if(oamScale > 256){
        oamScale = 256;
    }

    
    oamRotateScale(&oamSub, 1, 0,oamScale,oamScale);
    oamUpdate(&oamSub);
    bgSetScale(bgCanvas, scale, scale);
    bgSetScroll(bgCanvas,
                surf.x - (64 >> surf.z), // centrar en x=64..191
                surf.y);
    bgUpdate();
    updatePreviewPos();
}
//==================== PALETAS ==========|
// función auxiliar para esta situación específica
static void drawSliderRect(u16 *buf, int x, int row, int w, u16 color)
{
    u32 color32 = ((u32)color << 16) | color;
    u16 *base = buf + row * 64 + x;

    for (int j = 0; j < 8; j++)
    {
        u16 *p16 = base;
        int i = 0;

        if ((uintptr_t)p16 & 2)
        {
            *p16++ = color;
            i++;
        }

        u32 *p32 = (u32 *)p16;
        int w32 = (w - i) >> 1;
        for (int k = 0; k < w32; k++)
            *p32++ = color32;

        if ((w - i) & 1)
            *(u16 *)p32 = color;

        base += 64;
    }
}
ITCM_CODE static void draw4xRectIn64w(u16 *buf, int x, int y, u16 col)
{
    u32 col32 = ((u32)col << 16) | col;
    u32 *p = (u32 *)(buf + x + (y << 6));

    p[0] = col32;
    p[1] = col32;
    p += 32;
    p[0] = col32;
    p[1] = col32;
    p += 32;
    p[0] = col32;
    p[1] = col32;
    p += 32;
    p[0] = col32;
    p[1] = col32;
}

void drawNesPalette()
{
    for (int i = 0; i < 16 * 64; i++)
    {
        gfxRGBsliders[i] = C_BLACK;
    }
    // dibujar paleta
    for (int i = 0; i < 4; i++) // vertical
    {
        for (int j = 0; j < 16; j++) // horizontal
        {
            draw4xRectIn64w(gfxRGBsliders, j << 2, (i << 2) + 16, nesPalette[(i << 4) + j]);
        }
    }
}
ITCM_CODE void drawColorPalette()
{
    // dibujar paleta
    for (int i = 0; i < 16; i++) // vertical
    {
        for (int j = 0; j < 16; j++) // horizontal
        {
            draw4xRectIn64w(gfxPalette, j << 2, i << 2, palette[(i << 4) + j]);
        }
    }
}

void updatePal(int increment, int *palettePos)
{
    // primero debemos saber si estamos en un rango válido
    if (*palettePos + increment < 0 || *palettePos + increment > paletteSize - 1)
    {
        return;
    }

    // obtenemos la coordenada de la paleta
    int posx = *palettePos & 15;
    int posy = *palettePos >> 4;

    u16 _col = palette[*palettePos];

    // nueva información
    *palettePos += increment;

    _col = palette[*palettePos];
    // obtener cada color RGB
    u8 r = (_col & 31);
    u8 g = (_col & 992) >> 5;
    u8 b = (_col & 31744) >> 10;
    u8 _barColAmount[3] = {r, g, b};
    for (int i = 0; i < 3; i++)
        palEdit[i] = _barColAmount[i];

    if(!nesMode)
    {
        // rectangulos de abajo
        u16 _barCol[3] = {C_RED, C_GREEN, C_BLUE};

        for (int i = 0; i < 3; i++)
        {
            int y = (i << 3) + 8;
            drawSliderRect(gfxRGBsliders, 0, y, _barColAmount[i] << 1, _barCol[i]);
            drawSliderRect(gfxRGBsliders, _barColAmount[i] << 1, y, 64 - (_barColAmount[i] << 1), C_BLACK);
        }
        drawSliderRect(gfxRGBsliders, 0, 0, paletteAlpha, _col);
    }

    // obtenemos la coordenada de la paleta (otra vez)
    posx = *palettePos & 15;
    posy = *palettePos >> 4;

    if (paletteBpp != 8)
    {
        int prevOffset = paletteOffset;
        if (paletteBpp == 4)
        {
            paletteOffset = posy << 4;
        }
        else if (paletteBpp == 2)
        {
            paletteOffset = (posy << 4) + ((posx >> 2) << 2);
        }
        if (prevOffset != paletteOffset)
        { // se cambió la paleta, necesita actualizar la pantalla
            drawSurfaceMain();
        }
    }
    oamSetXY(&oamSub, paletteSelOamId, (posx << 2) + 191, (posy << 2) + 63);
    oamUpdate(&oamSub);
}
//esta función actualiza tanto el color como las barras de paletas
void updatePalEditBar(int index)
{
    static int prevAmount[4] = {0,0,0,0};
    if(prevAmount[index] == palEdit[index]){
        return;
    }
    int amount = palEdit[index];
    prevAmount[index] = amount;
    

    // Actualizar barra
    const u16 _barCol[3] = {C_RED, C_GREEN, C_BLUE};
    int y = (index << 3) + 8;
    drawSliderRect(gfxRGBsliders, 0, y, amount << 1, _barCol[index]);
    drawSliderRect(gfxRGBsliders, amount << 1, y, 64 - (amount << 1), C_BLACK);

    // Actualizar el color
    u16 _col = palEdit[0];
    _col += palEdit[1] << 5;
    _col += palEdit[2] << 10;
    _col |= 0x8000; // encender bit alpha
    palette[palettePos] = _col;

    draw4xRectIn64w(gfxPalette, (palettePos & 15) << 2, (palettePos >> 4) << 2, _col);

    if (paletteBpp != 16)
    {
        drawSurfaceMain();
        drawSliderRect(gfxRGBsliders, 0, 0, MAX_ALPHA, _col);
    }
    else
    {
        drawSliderRect(gfxRGBsliders, 0, 0, MAX_ALPHA, _col);
        drawSliderRect(gfxRGBsliders, paletteAlpha, 0, 64 - paletteAlpha, 0);
    }
    if (palettePos == 0)
    {
        if(showGrid == true){
            drawGrid(AVinvertColor(_col));
        }
        updatePreviewGfx();
    }
}

//=================================================================Inicialización===================================================================================|
void clearTop()
{
    memset(pixelsTop, 0, surfaceSize << 1);
}
void clearPal()
{
    for (int i = 0; i < paletteSize; i++)
    {
        palette[i] = C_BLACK;
    }
}
void clearAll()
{
    clearTop();
    clearPal();
    for (int i = 0; i < surfaceSize; i++)
    {
        surface[i] = 0;
        stack[i] = 0;
        pixelsTopVRAM[i] = 0;
    }
}

u16 *gfxBrushSettings;
u16 *gfx8;
void setBrushSettingsSprites(bool on)
{
    showBrushSettings = on;
    oamSet(&oamSub, brushSettingsOamId, //
           16, 32,                      // posición
           0,                           // prioridad
           on,                          // opaco
           SpriteSize_32x16, SpriteColorFormat_Bmp,
           gfxBrushSettings,
           -1,
           false, false, false, false, false);

    oamSet(&oamSub, brushSettingsSelectorOamId, //
           ((int)brushMode << 3) + 16, 32,      // posición
           0,                                   // prioridad
           on,                                  // opaco
           SpriteSize_8x8, SpriteColorFormat_Bmp,
           gfx8,
           -1,
           false, false, false, false, false);
    oamSet(&oamSub, brushSettingsSelectorOamId + 1, //
           ((int)brushSize << 3) + 16, 40,          // posición
           0,                                       // prioridad
           on,                                      // opaco
           SpriteSize_8x8, SpriteColorFormat_Bmp,
           gfx8,
           -1,
           false, false, false, false, false);
    oamUpdate(&oamSub);
}
void setOamBG()
{
    const u16 bgCol[2] = {0xA908, 0xA082};

    for(int y = 0; y < 32; y++){
        for(int x = 0; x < 32; x++){
            gfxBG[(y<<5) + x] = bgCol[(x + y) & 1];
        }
    }
    
    for (int i = 0; i < 16; i++)
    {
        int x = ((i & 0b11) << 5) + SURFACE_X;
        int y = (i & 0b1100) << 3;

        oamSet(&oamSub, i + 4, // index
               x, y,           // posición
               3,
               15, // opaco
               SpriteSize_32x32, SpriteColorFormat_Bmp,
               gfxBG,
               1,
               false, false, false, false, false);
    }
}
void updateIsActiveOam(){
    oamSet(&oamSub, isClipboardOamId,
        48, 16,
        0, hasClipboard,
        SpriteSize_16x16, SpriteColorFormat_Bmp,
        gfx16,
        -1,
        false, false, false, false, false); 
    oamSet(&oamSub, isAudioSyncOamId,
        208, 176,
        0, audioSync,
        SpriteSize_16x16, SpriteColorFormat_Bmp,
        gfx16,
        -1,
        false, false, false, false, false);
    oamSet(&oamSub, isGridOamId,
        240, 0,
        0, showGrid,
        SpriteSize_16x16, SpriteColorFormat_Bmp,
        gfx16,
        -1,
        false, false, false, false, false);
    oamSet(&oamSub, isOnionSkinOamId,
        240, 16,
        0, onionSkinEnable,
        SpriteSize_16x16, SpriteColorFormat_Bmp,
        gfx16,
        -1,
        false, false, false, false, false);
    oamUpdate(&oamSub);
}

void setEditorSprites()
{
    static bool initSprites = true;
    // iniciamos el sprite para dibujar : )
    oamInit(&oamMain,SpriteMapping_Bmp_1D_128, false);
    oamInit(&oamSub, SpriteMapping_Bmp_1D_128, false);

    oamClear(&oamMain, 0, 128);
    oamClear(&oamSub, 0, 128);

    if(initSprites == true){
        char device[16];
        fsGetDevice(device, sizeof(device));
        
        
        gfxPalette = oamAllocateGfx(&oamSub, SpriteSize_64x64, SpriteColorFormat_Bmp);
        gfx32 = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_Bmp);
        gfx16 = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_Bmp);
        gfx8 = oamAllocateGfx(&oamSub, SpriteSize_8x8, SpriteColorFormat_Bmp);
        gfx5 = oamAllocateGfx(&oamSub, SpriteSize_8x8, SpriteColorFormat_Bmp);
        gfxBrushSettings = oamAllocateGfx(&oamSub, SpriteSize_32x16, SpriteColorFormat_Bmp);
        gfxRGBsliders = oamAllocateGfx(&oamSub, SpriteSize_64x32, SpriteColorFormat_Bmp);
        gfxRgbSliderSel = oamAllocateGfx(&oamSub, SpriteSize_8x8, SpriteColorFormat_Bmp);
        gfxGrid = oamAllocateGfx(&oamSub, SpriteSize_64x64, SpriteColorFormat_Bmp);
        dmaFillWords(0, gfxGrid, 64 * 64 * 2);
        gfxSelectedZone = oamAllocateGfx(&oamMain, SpriteSize_64x64, SpriteColorFormat_Bmp);
        gfxBG = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_Bmp);
        if (!nitroFSInit(NULL))
            return;//ojalá que no falle

        importACS16("nitro:/selector24.acs",gfx32,stack);
        importACS16("nitro:/selector16.acs",gfx16,stack);
        importACS16("nitro:/selector8.acs",gfx8,stack);
        importACS16("nitro:/selector5.acs",gfx5,stack);
        importACS16("nitro:/brushSettings.acs",gfxBrushSettings,stack);
        importACS16("nitro:/rgbSliders.acs",gfxRGBsliders,stack);
        importACS16("nitro:/rgbSliderSel.acs",gfxRgbSliderSel,stack);
        importACS16("nitro:/rgbSliderSel.acs",gfxRgbSliderSel,stack);
        
        nitroFSExit();
        fsRestore(device,path);
        
    }
    updatePal(0,&palettePos);
    initSprites = false;

    oamSet(&oamSub, paletteOamId,
           192, 64,
           0,  // prioridad
           15, // palette
           SpriteSize_64x64, SpriteColorFormat_Bmp,
           gfxPalette,
           -1,
           false, false, false, false, false);

    oamSet(&oamSub, rgbSliderOamId,
           rgbSliderX, rgbSliderY,
           0,
           15, // opaco
           SpriteSize_64x32, SpriteColorFormat_Bmp,
           gfxRGBsliders,
           -1,
           false, false, false, false, false);

    oamSet(&oamSub, rgbSliderSelOamId,
           SCREEN_W - 2, rgbSliderY + 8,
           0,
           15, // opaco
           SpriteSize_8x8, SpriteColorFormat_Bmp,
           gfxRgbSliderSel,
           -1,
           false, false, false, false, false);

    oamSet(&oamSub, selector24oamID,
           0, 16,
           0,
           15, // opaco
           SpriteSize_32x32, SpriteColorFormat_Bmp,
           gfx32,
           -1,
           false, false, false, false, false);

    oamSet(&oamSub, paletteSelOamId,
           rgbSliderX - 1, 63,
           0,
           15, // opaco
           SpriteSize_8x8, SpriteColorFormat_Bmp,
           gfx5,
           -1,
           false, false, false, false, false);

    palettePos = 0;
    paletteOffset = 0;
    
    for(int i = gridOamId; i < 4; i++)
    {
        oamSetBlendMode(&oamSub, i,SpriteMode_Blended);
    }
    oamSetBlendMode(&oamMain,selectedZoneOamId,SpriteMode_Blended);

    const int eva = 5;
    const int evb = 8;
    const int evy = 10;

    // Motor principal (oamMain)
    REG_BLDCNT = BLEND_ALPHA
    | BLEND_SRC_SPRITE
    | BLEND_DST_BG3
    | BLEND_DST_BACKDROP;
    REG_BLDALPHA = BLDALPHA_EVA(eva) | BLDALPHA_EVB(evb);
    REG_BLDY     = BLDY_EVY(evy);

    // Motor secundario (oamSub)
    REG_BLDCNT_SUB    = BLEND_ALPHA | BLEND_SRC_SPRITE | BLEND_DST_BG3 | BLEND_DST_BACKDROP;
    REG_BLDALPHA_SUB  = BLDALPHA_EVA(eva) | BLDALPHA_EVB(evb);
    REG_BLDY_SUB      = BLDY_EVY(evy);

    oamSet(&oamMain, selectedZoneOamId,
        0, 0,
        0, 15,
        SpriteSize_64x64, SpriteColorFormat_Bmp,
        gfxSelectedZone,
        -1,
        false, false, false, false, false);
        
    #define gridOpacity 8
    // top-left
    oamSet(&oamSub, gridOamId,
        SURFACE_X, 0,
        0, 8,
        SpriteSize_64x64, SpriteColorFormat_Bmp,
        gfxGrid,
        -1,
        false, false, false, false, false);

    // top-right
    oamSet(&oamSub, gridOamId + 1,
        SURFACE_X + 64, 0,
        0, 8,
        SpriteSize_64x64, SpriteColorFormat_Bmp,
        gfxGrid,
        -1,
        false, false, false, false, false);

    // bottom-left
    oamSet(&oamSub, gridOamId + 2,
        SURFACE_X, 64,
        0, 8,
        SpriteSize_64x64, SpriteColorFormat_Bmp,
        gfxGrid,
        -1,
        false, false, false, false, false);

    // bottom-right
    oamSet(&oamSub, gridOamId + 3,
        SURFACE_X + 64, 64,
        0, 8,
        SpriteSize_64x64, SpriteColorFormat_Bmp,
        gfxGrid,
        -1,
        false, false, false, false, false);

    setOamBG();
    setBrushSettingsSprites(true);
    updatePreviewPos();
    updatePreviewGfx();
}

void initBitmap()
{
    clearAll();

    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE);

    consoleInit(&topConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 4, true, true);
    consoleSetFont(&topConsole, &font);
    oamClear(&oamSub, 0, 128);
    videoSetModeSub(MODE_5_2D); // pantalla inferior bitmap
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_SUB_SPRITE); // sprites en VRAM 
    // capas
    #if surfaceMaxExp <= 7
    bgInit(3, BgType_Bmp16, BgSize_B16_128x128, 0, 0);
    bgCanvas = bgInitSub(3, BgType_Bmp16, BgSize_B16_128x128, 4, 0);
    #else
    bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    bgCanvas = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 4, 0);
    #endif
    
    bgUI = bgInitSub(2, BgType_Bmp8, BgSize_B8_256x256, 0, 0);


    pixelsVRAM = (u16*)bgGetGfxPtr(bgCanvas);

    decompress(GFXinputBitmap, bgGetGfxPtr(bgUI), LZ77Vram);
    dmaCopyAsynch(GFXinputPal, BG_PALETTE_SUB, GFXinputPalLen);    

    drawSurfaceMain();
    drawSurfaceBottom();
    accurate = true;
    submitVRAM();

    // Prioridades: UI detrás del canvas
    bgSetPriority(bgCanvas, 0); // canvas prioridad mínima
    bgSetPriority(bgUI, 0);
    bgSetScale(3, 256, 256);
    //calcular offsets
    previewXoffset = (SCREEN_W-(surf.fw))>>1;
    previewYoffset = (SCREEN_H-(surf.fh))>>1;
    bgSetScroll(3, -previewXoffset, -previewYoffset);
    bgUpdate();

    setEditorSprites();
    setBackupVariables();
    paletteBpp = 8;
    surf.fw = 1<<surf.w;
    surf.fh = 1<<surf.h;
}
//====================================================================Backups==============================================================|
void setBackupVariables()
{
    backupIndex = 0;
    backupSize = surf.fw << surf.h;
    backupMax = BACKUP_SIZE/backupSize;
    
    // reinicia el backup
    for (int i = 0; i < BACKUP_SIZE; i++)
    {
        backup[i] = 0;
    }
}
void backupWrite()
{
    backupIndex++;
    if (backupIndex > backupMax)
    {
        backupIndex = 0;
    }
    int index = backupIndex * backupSize;
    // copia surface a backup+ su index
    dmaCopyHalfWordsAsynch(2, surface, backup + index, backupSize * sizeof(u16));
}
void backupRead()
{
    // Calculamos el índice del bloque en el array backup
    int index = backupIndex * backupSize;
    dmaCopyHalfWordsAsynch(2, backup + index, surface, backupSize * sizeof(u16));
    accurate = true;
}
//====================================================================Compatibilidad con modos gráficos====================================|
void textMode()
{
    if (currentSubMode == SUB_TEXT)
        return; // ya estamos en texto
    currentSubMode = SUB_TEXT;

    for(int i = 0; i < surfaceSize; i++){
        const u16 col = pixelsTopVRAM[i];
        const u8 r = ((col>>10) & 31);
        const u8 g = ((col>>5)  & 31);
        const u8 b = (col & 31);
        const u8 o = (r+g+b)>>4;
        pixelsTopVRAM[i] = (o<<10)|(o<<5)|(o)|0x8000;
    }
    consoleClear();
    oamClear(&oamSub, 0, 128);
    oamUpdate(&oamSub);
}
extern u16* orig;
extern int count;
void settingsMode(){
    
    backupWrite();
    updateSettings = true;
    consoleClear();

    oamClear(&oamSub, 0, 128);

    videoSetModeSub(MODE_0_2D);
    vramSetBankC(VRAM_C_SUB_BG); // banco de VRAM para BG del engine B

    consoleInit(&subConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
    consoleSetFont(&subConsole, &font);
    
    if (currentSubMode == SUB_TEXT)
        return; // ya estamos en texto
    currentSubMode = SUB_TEXT;
    currentConsoleMode = MODE_SETTINGS;

    
    // Definir claramente las unidades
    const u32 PALETTE_SIZE_BYTES = 256 * sizeof(u16);
    const u32 BACKUP_SIZE_U16 = BACKUP_SIZE / sizeof(u16); // Si BACKUP_SIZE está en bytes

    // Calcular posición de backup
    effectBackupPos = ((backupIndex + 1) * backupSize) / sizeof(u16);

    // Verificar espacio suficiente (todo en unidades de u16)
    if(effectBackupPos + surfaceSize + PALETTE_SIZE_BYTES > BACKUP_SIZE_U16){
        effectBackupPos = 0;
    }

    orig = backup + effectBackupPos;

    // Leer datos
    if(paletteBpp < 16){//indexiado
        for(int i = 0; i < 256; i++){
            orig[i] = palette[i];
        }
    } else {
        dmaCopy(surface, backup + effectBackupPos, surfaceSize * sizeof(u16));
    }
    runTextConsole();
}

int bgPreview;
void textKeyboardDraw()
{
    // añadir capa de preview
    dmaCopy(GFXconsoleInputPal, BG_PALETTE_SUB, GFXconsoleInputPalLen);
    bgPreview = bgInitSub(3, BgType_Bmp16, BgSize_B16_128x128, 4, 0);
    bgPreviewGfx = bgGetGfxPtr(bgPreview);
    dmaFillWords(0, bgPreviewGfx, 128 * 128 * 2);

    int bg2 = bgInitSub(2, BgType_Bmp8, BgSize_B8_256x256, 0, 0);

    decompress(GFXconsoleInputBitmap, bgGetGfxPtr(bg2), LZ77Vram);    

    bgSetScale(bgPreview, 296, 296);
    bgSetScroll(bgPreview, 0, 0);
    bgSetPriority(bgPreview, 0); // 0 = mayor prioridad
    bgSetPriority(bg2, 1);
    bgUpdate();

    printf(fname);
    printf(texts[selectorA]);
    printf("\n????????????????????????????????\n");
    listFiles();
    redraw = false;
}
void bitmapMode()
{
    dmaFillWords(0, pixelsTop, surfaceSize<<1);
    if (currentSubMode == SUB_BITMAP)
        return; // ya estamos en bitmap
    currentSubMode = SUB_BITMAP;
    surf.fw = 1<<surf.w;
    surf.fh = 1<<surf.h;
    // reiniciamos VRAM
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE); // sprites en VRAM B
    int bgMain = bgInit(3, BgType_Bmp16, BgSize_B16_128x128, 0, 0);
    consoleInit(&topConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 4, true, true);
    consoleSetFont(&topConsole, &font);

    oamClear(&oamMain, 0, 128);
    oamClear(&oamSub, 0, 128);

    videoSetModeSub(MODE_5_2D); // pantalla inferior bitmap
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_SUB_SPRITE); // sprites en VRAM D

    bgCanvas = bgInitSub(3, BgType_Bmp16, BgSize_B16_128x128, 4, 0);
    bgUI = bgInitSub(2, BgType_Bmp8, BgSize_B8_256x256, 0, 0);

    decompress(GFXinputBitmap, bgGetGfxPtr(bgUI), LZ77Vram);
    dmaCopyAsynch(GFXinputPal, BG_PALETTE_SUB, GFXinputPalLen);

    setEditorSprites();
    paletteAlpha = MAX_ALPHA;
    drawSurfaceMain();
    submitVRAM(true,false); // recuperamos nuestros queridos datos

    bgSetScale(bgCanvas, 256, 256);
    bgSetScroll(bgCanvas, -64, -32);

    previewXoffset = (SCREEN_W-(surf.fw))>>1;
    previewYoffset = (SCREEN_H-(surf.fh))>>1;
    bgSetScale(bgMain, 256, 256);
    bgSetScroll(bgMain, -previewXoffset, -previewYoffset);

    if(showGrid)
        {drawGrid(AVinvertColor(palette[paletteOffset]));}else{dmaFillWords(0, gfxGrid, 64 * 64 * 2);}
    drawSurfaceBottom();
    updateIsActiveOam();
    drawColorPalette();
}

#ifdef DEBUG_CPU
u32 cpuUsage;
u32 maxCpu;
u32 _cpuDebugIterations;
inline void calculateCpuUsage(u64 ticks)
{
    // Leer resultado del frame anterior
    cpuUsage = (u32)(REG_DIV_RESULT & 0xFFFFFFFF);
    
    // Iniciar nueva división para el próximo frame
    REG_DIV_NUMER = (s64)ticks;
    REG_DIV_DENOM = 56;
    REG_DIVCNT = 0;  // Iniciar división
}
#endif
const char bucketText[2][6] = {"Color", "Index"};
void drawInfo()
{
    // Guardar posición del cursor y moverlo al inicio (fila 0, columna 0)
    printf("\033[s\033[H");
    
#ifdef DEBUG_CPU
    calculateCpuUsage(frameEndTime - frameStartTime);
    timerStop();
    if(cpuUsage > maxCpu){
        maxCpu = cpuUsage;
    }
    if(_cpuDebugIterations > 29){
        u32 integer = cpuUsage / 100;
        u32 decimal = cpuUsage % 100;
        printf("\033[K%u.%02u%%", integer, decimal);
        _cpuDebugIterations = 0;
    }
    else{
        _cpuDebugIterations++;
    }
    timerContinue();
#endif
    
    if (animation.frames != 0) {
        printf("\n\033[Kframe: %d / %d \nanim speed: %d  ", 
               animation.pos, animation.frames, animation.speed);
    }

    if (bucketMode != 0) {
        printf("\n\033[KBucket: replace %s", bucketText[bucketMode - 1]);
    }
    printf("\033[u");
}
//============================================================= SD CARD ===============================================|

void createAppFolder(){
    mkdir("/_nds/", 0777);
    mkdir(APP_PATH, 0777);
}

void clearCache()
{
    // abrir el directorio
    DIR *dir = opendir(CACHE_PATH);
    if (dir)
    {
        struct dirent *entry;
        char filepath[256];

        while ((entry = readdir(dir)) != NULL)
        {
            // ignorar . y ..
            if (entry->d_name[0] == '.')
                continue;

            snprintf(filepath, sizeof(filepath), "%s%s", CACHE_PATH, entry->d_name);
            remove(filepath);
        }
        closedir(dir);
    }

    // recrear por si no existía
    mkdir(CACHE_PATH, 0777);
}

//============================================================= INPUT =================================================|
inline int getActionsFromKeys(int keys)
{
    int actions = ACTION_NONE;

    if (keys & KEY_UP)
        actions |= ACTION_UP;
    if (keys & KEY_DOWN)
        actions |= ACTION_DOWN;
    if (keys & KEY_LEFT)
        actions |= ACTION_LEFT;
    if (keys & KEY_RIGHT)
        actions |= ACTION_RIGHT;
    if (keys & KEY_A)
        actions |= ACTION_ZOOM_IN;
    if (keys & KEY_B)
        actions |= ACTION_ZOOM_OUT;

    return actions;
}

int getActionsFromTouch(int button)
{
    int actions = ACTION_NONE;

    switch (button)
    {
    case 1: actions |= ACTION_LEFT;     break;
    case 2: actions |= ACTION_RIGHT;    break;
    case 5: actions |= ACTION_UP;       break;
    case 6: actions |= ACTION_DOWN;     break;
    case 0: actions |= ACTION_ZOOM_IN;  break;
    case 4: actions |= ACTION_ZOOM_OUT; break;
    case 3:
        showGrid = !showGrid;
        if(showGrid)
            drawGrid(AVinvertColor(palette[paletteOffset]));
        else
            dmaFillWords(0, gfxGrid, 64 * 64 * 2);
        updateIsActiveOam();
        break;
    case 7:
        if(animation.frames > 0)
        {
            if(onionSkinEnable){
                onionSkinEnable = false;
            }else{
                void* dst = onionSkin;
                dmaFillWords((u32)(palette[0]<<16)|palette[0],dst, surfaceSize*2);
                onionSkinEnable = true;
            }
            drawSurfaceMain();
        }else{
            onionSkinEnable = false;
        }
        updateIsActiveOam();
        break;
    }

    return actions;
}

void applyActions(int actions)
{                                                         
    accurate = true;
    int blockSize = (surf.fw) >> surf.z;// tamaño de bloque en píxeles según el zoom
    if (kHeld & KEY_L || kHeld & KEY_X)
    {
        // --- Scroll por bloques ---
        if (actions & ACTION_UP)
            surf.y -= blockSize;previewPosAlpha = 15;
        if (actions & ACTION_DOWN)
            surf.y += blockSize;previewPosAlpha = 15;
        if (actions & ACTION_LEFT)
            surf.x -= blockSize;previewPosAlpha = 15;
        if (actions & ACTION_RIGHT)
            surf.x += blockSize;previewPosAlpha = 15;

        if (actions & ACTION_ZOOM_IN && gridSkips < surf.w)
        {
            gridSkips++;
        }
        if (actions & ACTION_ZOOM_OUT && gridSkips > 0)
        {
            gridSkips--;
        }
    }
    else
    {
        previewPosAlpha = 15;
        // --- Scroll por píxeles ---
        if (actions & ACTION_UP)
            surf.y--;
        if (actions & ACTION_DOWN)
            surf.y++;
        if (actions & ACTION_LEFT)
            surf.x--;
        if (actions & ACTION_RIGHT)
            surf.x++;

        if(moveCanvas == false){
        if (actions & ACTION_ZOOM_IN)
        {
            surf.z++;
            updatePreviewGfx();
        }
        if (actions & ACTION_ZOOM_OUT && surf.z > 0)
        {
            surf.z--;
            updatePreviewGfx();
        }
        }
    }
    drawSurfaceBottom();
    if(showGrid)
        drawGrid(AVinvertColor(palette[paletteOffset]));
}
void applyTool(int x, int y, bool dragging)
{
    if (x == prevx && y == prevy)
    {
        return;
    }
    prevx = x;
    prevy = y;

    u16 color = 0;
    if (paletteBpp != 16)
    {
        color = palettePos - paletteOffset;
    }
    else
    { // si estamos en modo 16 bits
        color = palette[palettePos];
        color = color | 0x8000; // forzar alpha
    }

    switch (currentTool)
    {
    case TOOL_BRUSH:
        if (dragging)
        {
            if (paletteAlpha != MAX_ALPHA)
            {
                if (paletteAlpha == 0)
                {
                    drawLineSurface(prevtpx, prevtpy, x, y, 0);
                    break;
                }
                drawLineSurfaceAlpha(prevtpx, prevtpy, x, y, color);
                break;
            }
            drawLineSurface(prevtpx, prevtpy, x, y, color);
        }
        else
        {
            if (paletteAlpha == 0)
            {
                brushStamp(x, y, 0);
                break;
            }
            brushStamp(x, y, color);
            break;
            // modo normal
            brushStamp(x, y, color);
        }
        break;

    case TOOL_ERASER:
        if (dragging)
        {
            drawLineSurface(prevtpx, prevtpy, x, y, 0);
        }
        else
        {
            drawPixelSurface(x, y, 0);
        }
        break;

    case TOOL_PICKER:
        if (paletteBpp == 16)
        {
            palette[palettePos] = surface[(y << surf.w) + x];
            // paletteAlpha = MAX_ALPHA;
            updatePal(0, &palettePos);
        }
        else
        {
            updatePal(surface[(y << surf.w) + x] - color, &palettePos);
        }
        currentTool = TOOL_BRUSH; // volver a seleccionar el pincel
        oamSetXY(&oamSub, selector24oamID, 0, 16);
        oamUpdate(&oamSub);
        break;

    case TOOL_BUCKET:
        if (paletteAlpha != MAX_ALPHA)
        {
            if (paletteAlpha == 0)
            {
                floodFill(surface, x, y, surface[(y << surf.w) + x], 0, surf.w, surf.h);
                break;
            }
            u16 _col = mergeColorAlpha(surface[(y << surf.w) + x], color, paletteAlpha);
            floodFill(surface, x, y, surface[(y << surf.w) + x], _col, surf.w, surf.h);
            break;
        }
        else
        {
            floodFill(surface, x, y, surface[(y << surf.w) + x], color, surf.w, surf.h);
        }
        break;
    }
}

//====================================================================MAIN==================================================================================================================|
int main(int argc, char *argv[])
{
    defaultExceptionHandler(); // Mostrar crasheos
    // Intentar montar la SD
    // Intentar montar primero con DLDI (para flashcards DS/DS Lite)
    if (argc < 2) {
        intro();
        surf.h = surfaceMaxExp;
        surf.w = surfaceMaxExp;
        surf.fh = 1<<surf.h;
        surf.fw = 1<<surf.w;
    }
    bool sd_ok = fatInitDefault();
    if (sd_ok)
    {
        // Intentar cambiar a fat:/ primero (flashcards)
        if (chdir("fat:/") == 0)
        {
            currentDir = opendir(".");
        }
        // Si falla, intentar sd:/ (DSi)
        #ifdef DSiMode
        else if (chdir("sd:/") == 0)
        {
            currentDir = opendir(".");
        }
        #endif
        // Como último recurso, usar la raíz actual
        else
        {
            currentDir = opendir(".");
        }
        createAppFolder();
        clearCache();
    }
    if (!sd_ok)
    {
        // --- Inicializar video temporalmente en modo consola (pantalla superior) ---
        videoSetMode(MODE_0_2D); // modo texto


        // poner pantalla inferior en modo texto temporal
        videoSetModeSub(MODE_0_2D);
        vramSetBankC(VRAM_C_SUB_BG);
        consoleDemoInit();

        printf("\x1b[31m\n"); // Rojo

        printf("ERROR: SD CARD NOT INITIATED.\n");
        printf("\x1b[38m\n"); // blanco
        printf("You cannot load or save files.\n\n");
        printf("Try launching from:\n");
        printf(" TwiglightMenu++.\n");
        #ifdef DSiMode
        if(isDSiMode()){
            printf(" Unlaunch (DSi).\n");
        }
        #endif

        printf("\nStarting in 3 seconds");
        for (int i = 0; i < 3; i++) // cantidad segundos
        {
            printf(".");
            for (int j = 0; j < 60; j++)
            {
                swiWaitForVBlank();
            }
        }
    }
    
    initGradient();
    initBitmap();
    #ifdef DEBUG_CPU
    initFPS();
    initTimers();
    #endif

    // aclarar la pantalla
    if (argc < 2) {
        for (int i = 0; i < 16; i++)
        {
            setBrightness(3, i - 15);
            swiWaitForVBlank();
        }
    }
    else{//se cargó un archivo
        const char *filePath = argv[1];
        struct stat fileStat;
        if (stat(filePath, &fileStat) != 0) {
            printf("Error: File not found!\n%s\n", filePath);
            goto programStart;
        }
        const char *extension = strrchr(filePath, '.');
        if (extension == NULL) {
            printf("Error: No extension found\n");
            goto programStart;
        }
        //comparar extensiones para abrir archivo
        memset(surface,0,surfaceBytes);
        memset(pixelsTopVRAM,0,surfaceBytes);
        const char *ext = getFileExtension(filePath);
        if (ext && strcmp(ext, "acs") == 0) {
            importACS(filePath,surface,palette);
        }else if (ext && strcmp(ext, "png") == 0){
            png_import(filePath,surface,palette);
        }else if (ext && strcmp(ext, "pcx") == 0){
            importPCX(filePath,surface,palette);
        }else if (ext && strcmp(ext, "bmp") == 0){
            loadBMP(filePath,surface,palette);
        }else if (ext && strcmp(ext, "wav") == 0){
            initAudio();
            wavPlay(filePath);
        }else if (ext && strcmp(ext, "gif") == 0){
            importGIF(filePath);
        }
        surf.fh = 1<<surf.h;
        surf.fw = 1<<surf.w;
        previewXoffset = (SCREEN_W-(surf.fw))>>1;
        previewYoffset = (SCREEN_H-(surf.fh))>>1;
        bgSetScroll(3, -previewXoffset, -previewYoffset);
        drawSurfaceMain();
        drawSurfaceBottom();
        drawColorPalette();
        submitVRAM(true,true);
    }
    programStart:
    //========================================================================WHILE LOOP!!!!!!!!!==========================================|
    while(1)
    {
        int actions = ACTION_NONE;
        // input
        scanKeys();
        kDown = keysDown();
        kHeld = keysHeld();
        kUp = keysUp();
        #ifdef DEBUG_CPU
            frameEndTime = timerRead();
        #endif
        drawInfo();
        #ifdef DEBUG_CPU
            timerReset();
            frameStartTime = timerRead();
        #endif
        // verificar si siquiera hay un input en este frame
        if((kUp | kHeld) == 0)
        {
            goto frameEnd;
        }

        if(kHeld | KEY_TOUCH){
            touchRead(&touch);
        }
        if (kUp & KEY_TOUCH)
        { // permitir volver a dibujar en un pixel
            prevx = -1;
            prevy = -1;
            stylusHoldTimer = STYLUSHOLDTIME;
            stylusRepeat = false;
        }
        if (kHeld & (KEY_L|KEY_X)) // zoom y offsets
        {
            actions |= getActionsFromKeys(kDown);
        }
        else // paleta de colores
        {
            if (kHeld & KEY_SELECT && nesMode == false)
            { // selector
                if (kDown & (KEY_UP | KEY_DOWN))
                {
                    if (kDown & KEY_UP)
                    {
                        palEditSel--;
                    }
                    else
                    {
                        palEditSel++;
                    }
                    palEditSel &= 3;
                    if (paletteBpp != 16 && palEditSel == 0)
                    {
                        palEditSel = 1;
                    }
                    // draw the rectangle with the selected bar.
                    oamSetXY(&oamSub, rgbSliderSelOamId, SCREEN_W - 2, (palEditSel << 3) + rgbSliderY);
                    oamUpdate(&oamSub);
                    goto frameEnd;// you can only use one input per frame, nothing more to check here!
                }
                if (kDown & (KEY_RIGHT | KEY_LEFT))
                {
                    if (palEditSel != 0)
                    {
                        u8 index = palEditSel - 1;
                        if (kDown & KEY_RIGHT)
                            palEdit[index]++;
                        else
                            palEdit[index]--;

                        palEdit[index] &= 31;
                        updatePalEditBar(index);
                    }
                    else
                    {
                        if (kDown & KEY_RIGHT)
                            paletteAlpha++;
                        else
                            paletteAlpha--;

                        paletteAlpha &= MAX_ALPHA;
                        // el color no cambia, pero la barra sí
                        drawSliderRect(gfxRGBsliders, 0, 0, MAX_ALPHA, palette[palettePos]);
                        drawSliderRect(gfxRGBsliders, paletteAlpha, 0, 64 - paletteAlpha, 0);
                    }
                    goto frameEnd;
                }
            }
            else
            {
                if (kDown & KEY_DOWN)
                {
                    updatePal(16, &palettePos);
                    goto frameEnd;
                }
                if (kDown & KEY_UP)
                {
                    updatePal(-16, &palettePos);
                    goto frameEnd;
                }
                if (kDown & KEY_LEFT)
                {
                    updatePal(-1, &palettePos);
                    goto frameEnd;
                }
                if (kDown & KEY_RIGHT)
                {
                    updatePal(1, &palettePos);
                    goto frameEnd;
                }
            }
        }
        if (kDown & KEY_R || kDown & KEY_Y)
        {
            showGrid = !showGrid;
            if(showGrid)
            {drawGrid(AVinvertColor(palette[paletteOffset]));}else{dmaFillWords(0, gfxGrid, 64 * 64 * 2);}
            updateIsActiveOam();
            goto frameEnd;
        }
        //===========================================PALETAS=========================================================
        palettePos = palettePos & (paletteSize - 1); // mantiene la paleta dentro de un límite
        if (palettePos < 0)
        {
            palettePos = 0;
        }
        if (kHeld & KEY_TOUCH)
        {
            if (stylusHoldTimer > 0)
            {
                stylusHoldTimer--;
            }
            else
            {
                stylusRepeat = true;
            }
            if (touch.px >= SURFACE_X && touch.px < (SURFACE_W + SURFACE_X))
            { // TOUCH EN EL CENTRO!
                if (touch.py <= SURFACE_H && (kDown & KEY_TOUCH || repeatCanvas))
                {// APUNTA A LA SURFACE!
                    imgChanges = true;
                    repeatCanvas = true;
                    int localX = touch.px - SURFACE_X;
                    int localY = touch.py;

                    int srcX = surf.x + (localX >> surf.z);
                    int srcY = surf.y + (localY >> surf.z);

                    if (srcY < surf.fh) // comprobar si está en el rango (solo por si acaso)
                    {
                        if (!(stylusPressed && prevtpx == srcX && prevtpy == srcY))
                        {
                            applyTool(srcX, srcY, stylusPressed);
                            prevtpx = srcX;
                            prevtpy = srcY;
                            drawSurfaceMain();
                            drew = true;
                            stylusPressed = true;
                        }
                        goto frameEnd;
                    }
                }
                else
                {
                    // apunta a los botones de abajo
                    int row = (touch.py - SURFACE_H) >> 4;
                    int col = (touch.px - SURFACE_X) >> 4;
                    // PLACEHOLDER
                    if (row == 3 && (stylusPressed == false || stylusRepeat == true))
                    {
                        stylusPressed = true;
                        switch(col)
                        {
                        case 0: // delete frame
                            deleteAnimFrame();
                            break;

                        case 1: // add frame
                            insertAnimFrame();
                            break;

                        case 2: // prev frame
                            prevAnimFrame();
                            break;

                        case 3: // play animation
                            animation.isPlaying = true;
                            playAnimation();
                            drawSurfaceBottom();
                            break;

                        case 5: // next frame
                            nextAnimFrame();
                            break;

                        case 6: // less speed
                            if (animation.speed > 1)
                                animation.speed--;
                            break;

                        case 7: // more speed
                            animation.speed++;
                            break;
                        }
                    }
                    if(stylusRepeat == true){
                        for(int i = 0; i<=animation.speed; i++){
                            swiWaitForVBlank();
                        }
                    }
                    goto frameEnd;
                }
            }
            if (touch.px < 64) // apunta a la parte izquierda
            {
                if (touch.px < 48 && touch.py > 16 && touch.py < 64 && stylusPressed == false) // herramientas
                {
                    if (showBrushSettings && touch.px >= 16 && touch.px < 48 && touch.py >= 32 && touch.py < 48)
                    { // si está en modo configurar brush
                        int col = (touch.px - 16) >> 3;
                        int row = (touch.py - 32) >> 3;
                        stylusPressed = true;
                        switch (row)
                        {
                        case 0: // patrones
                            brushMode = (BrushMode)col;
                            break;

                        case 1: // tamaños
                            brushSize = (BrushSize)col;
                            break;
                        }
                        setBrushSettingsSprites(true);
                        goto frameEnd;
                    }
                    int col = touch.px > 24 ? 1 : 0;
                    int row = touch.py > 40 ? 2 : 0;
                    // convertir col+row a un valor único
                    ToolType prevTool = currentTool;
                    currentTool = (ToolType)(row + col);
                    if (currentTool == prevTool && currentTool == TOOL_BUCKET)
                    {
                        bucketMode++;
                        if (bucketMode > 2)
                        {
                            bucketMode = 0;
                        }
                        consoleClear();
                    }
                    if (currentTool == TOOL_BRUSH)
                    {
                        setBrushSettingsSprites(true);
                    }
                    else
                    {
                        setBrushSettingsSprites(false);
                    }
                    // además dibujamos un contorno en dónde seleccionamos
                    oamSetXY(&oamSub, selector24oamID, col * 24, (row * 12) + 16);
                    oamUpdate(&oamSub);
                    stylusPressed = true;
                    goto frameEnd;
                }
                else // apunta a otra parte de la izquierda
                {
                    if (touch.py < 16 && stylusPressed == false)
                    { // iconos de la parte superior
                        int selected = touch.px >> 4;
                        fname[0] = '\0'; // quitar nombre reciente :>
                        // actualizar input para que la pantalla también lo haga
                        kDown = kDown | KEY_TOUCH;
                        switch (selected)
                        {
                        case 0: // load file
                            textMode();
                            currentConsoleMode = LOAD_file;
                            textKeyboardDraw();
                            runTextConsole();
                        break;
                        case 1: // New file
                            textMode();
                            currentConsoleMode = MODE_NEWIMAGE;
                            decompress(GFXnewImageInputBitmap, BG_GFX_SUB, LZ77Vram);
                            dmaCopy(GFXnewImageInputPal, BG_PALETTE_SUB, GFXnewImageInputPalLen);
                            runTextConsole();
                        break;
                        case 2: // Save file
                            textMode();
                            currentConsoleMode = SAVE_file;
                            textKeyboardDraw();
                            runTextConsole();
                        break;
                        case 3: // Open config
                            settingsMode();
                        break;
                        }
                    }
                    // botones del costado derecho en la izquierda
                    else if (touch.px >= 48 && touch.py < 64 && stylusPressed == false)
                    {
                        int selected = touch.py >> 4;
                        switch (selected) // puro hardcode lol
                        {
                        case 1: // Copy
                            copyFromSurfaceToStack();
                            break;
                        case 2: // cut
                            cutFromSurfaceToStack();
                            drawSurfaceMain();
                            break;
                        case 3: // Paste
                            pasteFromStackToSurface();
                            drawSurfaceMain();
                            break;
                        }
                        updateIsActiveOam();
                        stylusPressed = true;
                        goto frameEnd;
                    }
                    if(touch.py >= 64 && (stylusPressed == false || stylusRepeat == true))
                    {// revisar botones inferiores
                        // hardcodeado porque lol
                        int selected = touch.px >> 4;
                        selected += ((touch.py - 64) >> 4) << 2;
                        stylusPressed = true;
                        switch (selected)
                        {
                        case 0: // rotate -90°
                            rotateNegative();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 1: // rotate 90°
                            rotatePositive();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 2:
                            flipV();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 3:
                            flipH();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 6:
                            // verificar si es posible escalar
                            scaleUp();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 7:
                            scaleDown();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 8:
                            shiftLeftWrap();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 9:
                            shiftRightWrap();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 10:
                            shiftUpWrap();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 11:
                            shiftDownWrap();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 24: // Page UP
                        case 25: // Page DOWN
                        {
                            if (usesPages)
                            {
                                if(imgChanges){
                                    saveFile(imgFormat, currentFilePath, palette, surface);
                                }
                                
                                int dir = (selected == 24) ? -1 : +1;
                                fileOffset += dir * (paletteBpp << 11);

                                loadFile(imgFormat, currentFilePath, palette, surface);
                                drawSurfaceMain();
                                accurate = true;
                            }
                        }
                            goto frameEnd;

                        case 26: // undo
                            backupIndex--;
                            if (backupIndex < 0)
                            {
                                backupIndex = backupMax;
                            }
                            backupRead();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 27: // redo
                            backupIndex++;
                            if (backupIndex > backupMax)
                            {
                                backupIndex = 0;
                            }
                            backupRead();
                            drawSurfaceMain();
                            goto frameEnd;
                        }
                    }
                }
            }
            // zona de paletas y otras configuraciones
            if (touch.px >= 192) // apunta a la parte derecha
            {
                if (touch.py < 32 && (stylusPressed == false || stylusRepeat == true))
                {                                    // botones superiores
                    int col = (touch.px - 192) >> 4; // 0..3
                    int row = touch.py >> 4;         // 0..1
                    if ((unsigned)col < 4 && (unsigned)row < 2)
                    {
                        int button = (row << 2) | col; // 0..7
                        actions |= getActionsFromTouch(button);
                    }
                    stylusPressed = true;
                    goto frameEnd;
                }
                else if (touch.py < 40 && touch.py > 32 && repeatCanvas == false)
                { // transparencia
                    if(paletteBpp == 16){
                        paletteAlpha = (touch.px - 192);
                        drawSliderRect(gfxRGBsliders, 0, 0, MAX_ALPHA, palette[palettePos]);
                        drawSliderRect(gfxRGBsliders, paletteAlpha, 0, 64 - paletteAlpha, 0);
                    }else{
                        if(kDown & KEY_TOUCH){
                            if(palette[palettePos] & 0x8000){
                                palette[palettePos] &= 0x7FFF;
                            }
                            else{
                                palette[palettePos] |= 0x8000;
                            }
                            drawSliderRect(gfxRGBsliders, 0, 0, MAX_ALPHA,palette[palettePos]);
                            drawSurfaceMain();
                            accurate = updated = true;
                        }
                    }
                    
                    goto frameEnd;
                }
                else if (touch.py >= 40 && touch.py < 64 && repeatCanvas == false) // creador de colores
                {
                    imgChanges = true;
                    // hay mucho código hardcodeado aquí para mejorar el rendimiento :>
                    if(nesMode)
                    {
                        int ystart = 48;
                        int row = (touch.px - 192) >> 2;
                        int col = (touch.py - ystart) >> 2;
                        int index = (col << 4) + row;

                        u16 _col = nesPalette[index];
                        palette[palettePos] = _col;
                        draw4xRectIn64w(gfxPalette, (palettePos & 15) << 2, (palettePos >> 4) << 2, _col);
                        drawSurfaceMain();

                        goto frameEnd;
                    }
                    else
                    { // creador de colores en modo
                        int index = (touch.py - 40) >> 3;
                        palEdit[index] = (touch.px - 192) >> 1;

                        updatePalEditBar(index);

                        goto frameEnd;
                    }
                }
                else if (touch.py > 128 && stylusPressed == false)
                { // botones inferior derecha
                    // obtenemos el indice a base de donde apretamos
                    int row = (touch.px - 192) >> 4;
                    int col = (touch.py - 128) >> 4;
                    int pos = row+(col<<2);
                    switch (pos)
                    {
                    case 0:
                        copyPalette();
                        drawColorPalette();
                        updatePal(0, &palettePos);
                        drawSurfaceMain();
                    break;

                    case 1:
                        pastePalette();
                        drawColorPalette();
                        updatePal(0, &palettePos);
                        drawSurfaceMain();
                    break;

                    case 2:
                        copyColor();
                        drawColorPalette();
                        updatePal(0, &palettePos);
                        drawSurfaceMain();
                    break;

                    case 3:
                        pasteColor();
                        drawColorPalette();
                        updatePal(0, &palettePos);
                        drawSurfaceMain();
                    break;

                    case 13:
                        audioSync = !audioSync;
                        updateIsActiveOam();
                    break;

                    case 14:
                        if(wavPlaying){
                            wavStop();
                         }else{
                            wavContinue();
                        }
                    break;

                    case 15:
                        textMode();
                        currentConsoleMode = LOAD_music;
                        textKeyboardDraw();
                        runTextConsole();
                    break;
                    }
                    stylusPressed = true;
                    goto frameEnd;
                }
                else
                { // seleccionar un color en la paleta
                    if (stylusPressed == false)
                    {
                        int row = (touch.py - 64) >> 2;
                        int col = (touch.px - 192) >> 2;

                        updatePal(((row << 4) + col) - palettePos, &palettePos);
                        stylusPressed = true;
                    }
                    goto frameEnd;
                }
            }
        }
        else
        {
            stylusPressed = false;
        }
        repeatCanvas = false;
    frameEnd:
        //actualizamos el coso del preview
        if(previewPosAlpha > 0){
            previewPosAlpha--;
            updatePreviewPos();
        }
        
        //updateFPS();

        if (kUp & KEY_TOUCH && drew == true)
        {
            drew = false;
            backupWrite();
        }
        if (actions != ACTION_NONE)
        {
            applyActions(actions);
        }
        //final del frame, actualizar música
        wavStreamUpdate();

        #ifdef DEBUG_CPU
        timerStop();
        swiWaitForVBlank();//ya no hay modo reposo ya que si no hay input no se hace nada.
        timerContinue();
        #else
        swiWaitForVBlank();
        #endif
        if (updated)
        { // llamar a submitVRAM solo si se modificó algo visual
            submitVRAM(accurate);
        }
        updated = false;
        accurate = false;
        // fin del loop de modo bitmap (pantalla de abajo)
    }
    /*
    Esta sección está hecha para quienes leyeron el código!

    solo voy a decir que estoy trabajando en ordenar un poco este código
    - Alfombra de marzo
    */
    return 0;
}