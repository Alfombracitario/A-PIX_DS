//Este código, es como una "librería" ya que la utlizaré para todos mis proyectos de DS en C/C++
/*
    Alfombra's visual DS library.
*/
#include <nds.h>
#include "avdslib.h"
// Cosas comunes
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MAX3(a, b, c) ((a) > (b) ? ((a) > (c) ? (a) : (c)) : ((b) > (c) ? (b) : (c)))
#define MIN3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))


u16 AVARGB(int r, int g, int b, int a) {
    return ((a & 1) << 15)       // Alpha en bit 15
         | ((b & 31) << 10)      // Azul
         | ((g & 31) << 5)       // Verde
         |  (r & 31);            // Rojo
}

u16 AVinvertColor(u16 color)
{
    return (~color) | 0x8000; //recordemos que alpha debe ser 1
}

void AVfillDMA(u16 *arr, int start, int end, u16 value) {
    int count = end - start;
    dmaFillHalfWords(value, &arr[start], count << 1);  
}

//específico para dibujado
void AVsetPixel(u16* arr, int x, int y, u16 color) {
    arr[(y<<8) + x] = color;
}

u16 AVreadPixel(u16* arr, int x, int y)
{
    return arr[(y<<8) + x];
}

void AVdrawRectangle(u16* arr, int x, int width, int y, int height, u16 color)
{
    int xlimit = x + width;
    int ylimit = y + height;
    int stride = 256; // ancho fijo del buffer

    if (width <= 8) {
        for (int i = y; i < ylimit; i++) {
            u16* row = arr + (i * stride) + x;
            for (int j = 0; j < width; j++)
                row[j] = color;
        }
        return;
    }

    u16 tempLine[256];
    for (int j = 0; j < width; j++)
        tempLine[j] = color;

    for (int i = y; i < ylimit; i++) {
        u16* row = arr + (i * stride) + x;
        memcpy(row, tempLine, width * 2);
    }
}

void AVdrawRectangleDMA(u16* arr, int x, int width, int y, int height, u16 color,int arrayXres) {
    int xto = x + width;
    height += y;
    for (int i = y; i < height; i++) {
        int _i = i << arrayXres;
        dmaFillHalfWords(color, &arr[_i + x], (xto - x) << 1);
    }
}

void AVdrawRectangleHollow(u16* arr, int x, int width, int y, int height, u16 color) {
    int xlimit = x + width;
    int ylimit = y + height;

    // línea superior e inferior
    for (int i = x; i < xlimit; i++) {
        arr[(y << 8) + i]         = color;       // top
        arr[((ylimit - 1) << 8) + i] = color;    // bottom
    }

    // líneas laterales
    for (int j = y + 1; j < (ylimit - 1); j++) {
        arr[(j << 8) + x]         = color;       // left
        arr[(j << 8) + (xlimit - 1)] = color;    // right
    }
}

void AVdrawVline(u16* arr,int y0, int y1, int x, u16 color){
    y0 = (y0<<8)+x;
    y1 = (y1<<8)+x;
    for(int i = y0; i < y1; i+=256)
    {
        arr[i] = color;
    }
}
void AVdrawHline(u16* arr, int x0, int x1, int y, u16 color) {
    y = y<<8;
    for(int i = x0; i < x1; i++){
        arr[i+y] = color;
    }
}

void AVdrawHlineDMA(u16* arr, int x0, int x1, int y, u16 color) {
    dmaFillHalfWords(color, &arr[(y << 8) + x0], (x1 - x0) << 1);
}
u32 AVRGBTOHSV(u8 r, u8 g, u8 b){
    u8 max = MAX3(r,g,b);
    u8 min = MIN3(r,g,b);
    u8 d = max - min;
    u8 h, s, v;
    
    v = max;
    
    if(d == 0){
        s = 0;
        h = 0; // Hue indefinido
    }else{
        s = (d * 255) / max; // Saturación 0-255
        
        // Calcular Hue 0-255 (mapeado a 0-360°)
        if(max == r){
            h = ((g - b) * 255) / (6 * d);
            // Ajuste para valores negativos
            if(h < 0) h += 255;
        }else if(max == g){
            h = ((b - r) * 255) / (6 * d) + 85; // 85 = 120/360 * 255
        }else{ // max == b
            h = ((r - g) * 255) / (6 * d) + 170; // 170 = 240/360 * 255
        }
    }
    
    return (h << 16) | (s << 8) | v;
}

u16 AVHSVTORGB(u8 h, u8 s, u8 v){
    if(s == 0){
        u8 gray = v >> 3; // Escala de grises a 5 bits
        return 0x8000 | (gray << 10) | (gray << 5) | gray;
    }
    
    // Mejor usar multiplicación para más precisión
    u32 sector_tmp = (h * 6) / 256; // 0-5
    u8 sector = sector_tmp & 0xFF;
    
    // Remainder dentro del sector (0-255)
    u32 remainder_tmp = (h * 6) % 256;
    u8 remainder = remainder_tmp & 0xFF;
    
    // Cálculos de interpolación
    u8 p = (v * (255 - s)) >> 8;
    u8 q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    u8 t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    
    u8 r, g, b;
    
    switch(sector){
        case 0: // 0-60°
            r = v; g = t; b = p; break;
        case 1: // 60-120°
            r = q; g = v; b = p; break;
        case 2: // 120-180°
            r = p; g = v; b = t; break;
        case 3: // 180-240°
            r = p; g = q; b = v; break;
        case 4: // 240-300°
            r = t; g = p; b = v; break;
        default: // 300-360°
            r = v; g = p; b = q; break;
    }
    
    // Convertir a ABGR1555 (5 bits por canal)
    return 0x8000 | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
}