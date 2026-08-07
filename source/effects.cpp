#include <climits>
#include "effects.h"
#include "avdslib.h"


#define clamp(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

extern void drawSurfaceMain();
extern void submitVRAM(bool _accurate);
extern void copyFromSurfaceToStack();
extern void pasteFromStackToSurface();
extern u32 effectBackupPos;
extern u32 kDown;

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

bool applyEffect(EffectId id)
{
    u16* orig;
    u16* pixels;
    int count;

    if(paletteBpp < 16){
        pixels = palette;
        orig = backup + effectBackupPos + surfaceSize; // offset ya en u16, sin *2
        count = 256;
    } else {
        pixels = surface;
        orig = backup + effectBackupPos;
        count = surfaceSize;
    }
    int param = effects[id].paramValue;

    switch(id){
        case EFFECT_INVERT: {
            for(int i = 0; i < count; i++){
                pixels[i] = AVinvertColor(orig[i]);
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
            copyFromSurfaceToStack();
            if(surfaceYres > 3){
                surfaceYres--;
            }
            pasteFromStackToSurface();
            break;
        }

        case EFFECT_WCROP: {
            copyFromSurfaceToStack();
            if(surfaceXres > 3){
                surfaceXres--;
            }
            pasteFromStackToSurface();
            break;
        }

        case EFFECT_HEXPAND: {
            copyFromSurfaceToStack();
            if(surfaceYres < 7){
                surfaceYres++;
            }
            pasteFromStackToSurface();
            break;
        }

        case EFFECT_WEXPAND: {
            copyFromSurfaceToStack();
            if(surfaceXres < 7){
                surfaceXres++;
            }
            pasteFromStackToSurface();
            break;
        }

        case EFFECT_TO16BPP: {
            if(paletteBpp == 16){
                break;
            }
            paletteBpp = 16;
            const u32 iterations = 1<<surfaceXres<<surfaceYres;
            for(int i = 0; i < iterations; i++){
                surface[i] = palette[surface[i]];
            }

        }

        case EFFECT_POSTERIZE: {
        
            break;
        }
        default:
            return false;
    }

    drawSurfaceMain();
    submitVRAM(true);
    return true;
}