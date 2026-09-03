#include <climits>
#include "effects.h"
#include "avdslib.h"
#include "formatsglobals.h"

#define clamp(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

extern void drawSurfaceMain();
extern void submitVRAM(bool _accurate, bool _wait);
extern void copyFromSurfaceToStack();
extern void pasteFromStackToSurface();
extern u32 effectBackupPos;
extern u32 kDown;
extern u32 kUp;
extern u16 stack[surfaceSize];

EffectEntry effects[EFFECT_COUNT] = {
    { "Invert Colors"  ,    true,  0,   0,  0   },
    { "Set Grayscale"  ,    true,  0,   0,  0   },
    { "Set Brightness" ,    false, -30, 30, 0   },
    { "Set Contrast"   ,    false, -50, 50, 0   },
    { "Crop width"     ,    true,  0,   0,  0   },
    { "Crop height"    ,    true,  0,   0,  0   },
    { "Expand width"   ,    true,  0,   0,  0   },
    { "Expand height"  ,    true,  0,   0,  0   },
    { "Convert to 16bpp",   true,  0,   0,  0   },
    { "Reduce colors"  ,    false, 2,   255,255 }
};

u16* orig;
u16* pixels;
int count;

struct ColorEntry {
    u16 rgb1555;
    u32 frequency;
    bool kept;
};

//helpers (itcm)
__attribute__((section(".itcm"))) void indexedToDirect(){
    paletteBpp = 16;
    const u32 iterations = 1<<surf.w<<surf.h;
    for(int i = 0; i < iterations; i++){
        surface[i] = palette[surface[i]];
    }
}
//test de ahora, luego veo de dónde saco toda esta memoria (usando backup)
u16 temp[65536];
static u8 remapTable[32768];  // Tabla de remapeo separada
__attribute__((section(".itcm"))) void posterize(int numColors) {
    dmaFillHalfWords(0, temp, 65536*2);

    const int res = 1<<surf.w<<surf.h;
    
    // Contar frecuencias
    for(int i = 0; i < res; i++){
        temp[surface[i] & 0x7FFF]++;
    }

    // Recopilar colores únicos
    u16* uniqueColors = (u16*)(temp + 32768);
    int colorCount = 0;
    for(int i = 0; i < 32768; i++){
        if(temp[i] > 0){
            uniqueColors[colorCount] = i;
            colorCount++;
        }
    }

    // Ordenar por frecuencia
    for(int i = 0; i < colorCount - 1; i++){
        for(int j = i + 1; j < colorCount; j++){
            if(temp[uniqueColors[j]] > temp[uniqueColors[i]]){
                u16 tempColor = uniqueColors[i];
                uniqueColors[i] = uniqueColors[j];
                uniqueColors[j] = tempColor;
            }
        }
    }

    // Seleccionar paleta
    int paletteSize = (colorCount < numColors) ? colorCount : numColors;
    for(int i = 0; i < paletteSize; i++){
        palette[i] = uniqueColors[i] | 0x8000;
    }
    
    for(int i = paletteSize; i < 256; i++){
        palette[i] = 0x8000;
    }

    //incializamos el remapTable
    for(int i = 0; i < 32768; i++) {
        remapTable[i] = 0xFF;  // 0xFF = no asignado
    }

    // Asignar colores de la paleta
    for(int i = 0; i < paletteSize; i++) {
        remapTable[palette[i] & 0x7FFF] = i;
    }

    // ahora remapeamos los colores que no "clasificaron" a otro cercano en la paleta
    for(int i = paletteSize; i < colorCount; i++) {
        int color = uniqueColors[i];
        int r1 = (color >> 10) & 0x1F;
        int g1 = (color >> 5) & 0x1F;
        int b1 = color & 0x1F;
        
        int bestIndex = 0;
        int bestDistance = INT_MAX;
        
        for(int j = 0; j < paletteSize; j++) {
            int palColor = palette[j] & 0x7FFF;
            int r2 = (palColor >> 10) & 0x1F;
            int g2 = (palColor >> 5) & 0x1F;
            int b2 = palColor & 0x1F;
            
            int dr = r1 - r2;
            int dg = g1 - g2;
            int db = b1 - b2;
            int distance = dr*dr + dg*dg + db*db;
            
            if(distance < bestDistance) {
                bestDistance = distance;
                bestIndex = j;
                if(distance == 0) break;
            }
        }
        
        remapTable[color] = bestIndex;
    }

    // Remapear imagen
    for(int i = 0; i < res; i++){
        int color = surface[i] & 0x7FFF;
        if(remapTable[color] == 0xFF) {
            remapTable[color] = 0; //fallback
        }
        surface[i] = remapTable[color];
    }
    paletteBpp = 8;
}
bool applyEffect(EffectId id)
{
    if(paletteBpp == 16){
        pixels = surface;
        count = surfaceSize;
    }
    else{
        count = 256;
        pixels = palette;//destino
    }
    //sí, medio sucio pero esta parte está llena de placeholders lol
    int param = effects[id].paramValue;

    switch(id){
        case EFFECT_INVERT: {
            for(int i = 0; i < count; i++){
                pixels[i] = ~orig[i] | 0x8000;
            }
            break;
        }

        case EFFECT_GRAYSCALE: {
            for(int i = 0; i < count; i++){
                u16 c = orig[i];
                u8 r = c & 31;
                u8 g = (c>>5) & 31;
                u8 b = (c>>10) & 31;
                u8 p = (r+g+b)/3; //confiando que el compilador va a optimizar esto con una multiplicación
                pixels[i] = (p<<10)|(p<<5)|p|0x8000;
            }
            break;
        }

        case EFFECT_BRIGHTNESS:{
            s8 a = param; // -31 a 31
            for(int i = 0; i < count; i++){
                u16 c = orig[i];
                int r = (c & 0x1F) + a;
                int g = ((c >> 5) & 0x1F) + a;
                int b = ((c >> 10) & 0x1F) + a;
                r = clamp(r, 0, 31);
                g = clamp(g, 0, 31);
                b = clamp(b, 0, 31);
                pixels[i] = ((u8)b << 10) | ((u8)g << 5) | (u8)r | 0x8000;
            }
            break;
        }

        case EFFECT_CONTRAST:{
            s8 a = param+50;
            const int PIVOT = 16;      // punto medio del canal de 5 bits (0-31)
            const int SHIFT = 6;       // precisión fixed-point
            int factor = (a * (1 << SHIFT)) / 50; // a=50 -> factor=64 -> x1.0

            for(int i = 0; i < count; i++){
                u16 c = orig[i];
                int r = c & 0x1F;
                int g = (c >> 5) & 0x1F;
                int b = (c >> 10) & 0x1F;

                r = PIVOT + (((r - PIVOT) * factor) >> SHIFT);
                g = PIVOT + (((g - PIVOT) * factor) >> SHIFT);
                b = PIVOT + (((b - PIVOT) * factor) >> SHIFT);

                r = clamp(r, 0, 31);
                g = clamp(g, 0, 31);
                b = clamp(b, 0, 31);

                pixels[i] = ((u8)b << 10) | ((u8)g << 5) | (u8)r | 0x8000;
            }
            break;
        }

        case EFFECT_HCROP: {
            surf.x = 0;
            surf.y = 0;
            copyFromSurfaceToStack();
            if(surf.h > 3){
                surf.h--;
                surf.fh = 1<<surf.h;
            }
            dmaFillWords(0, surface, 128 * 128 * 2);
            pasteFromStackToSurface();
            break;
        }

        case EFFECT_WCROP: {
            surf.x = 0;
            surf.y = 0;
            copyFromSurfaceToStack();
            if(surf.w > 3){
                surf.w--;
                surf.fw = 1<<surf.w;
            }
            dmaFillWords(0, surface, 128 * 128 * 2);
            pasteFromStackToSurface();
            break;
        }

        case EFFECT_HEXPAND: {
            copyFromSurfaceToStack();
            if(surf.h < 7){
                surf.h++;
                surf.fh = 1<<surf.h;
            }
            pasteFromStackToSurface();
            break;
        }

        case EFFECT_WEXPAND: {
            copyFromSurfaceToStack();
            if(surf.w < 7){
                surf.w++;
                surf.fw = 1<<surf.w;
            }
            pasteFromStackToSurface();
            break;
        }

        case EFFECT_TO16BPP: {
            if(paletteBpp == 16){
                break;
            }
            indexedToDirect();
            break;
        }

        case EFFECT_POSTERIZE: {
            
            if(paletteBpp < 16){
                indexedToDirect();
                pixels = surface;
                orig = backup + effectBackupPos;
                count = surfaceSize;
            }
            posterize(param);
            break;
        }
        default:
            return false;
    }

    drawSurfaceMain();
    submitVRAM(true,false);
    return true;
}