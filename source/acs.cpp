#include "acs.h"

//This is the reader of ACS for A-pix DS, this was optimized to work better on this app using shortcuts.
#define ACScolModeARGB1555   0
#define ACScolModeARGB8888   1
#define ACScolModeRGB888     2
#define ACScolModeGrayScale8 3
#define ACScolModeGrayScale4 4
#define ACStotalModes 5

#define ACStypeusesPalette    1
#define ACStypeIsAnimated    (1<<1)//para algún futuro

#define ACSmirror 01
#define ACSpattern 00
#define ACSrepeat 1

static inline uint32_t log2(uint32_t x) {
    uint32_t r = 0;
    if (x >= 0x10000) { x >>= 16; r += 16; }
    if (x >= 0x100)   { x >>= 8;  r += 8;  }
    if (x >= 0x10)    { x >>= 4;  r += 4;  }
    if (x >= 0x4)     { x >>= 2;  r += 2;  }
    if (x >= 0x2)     {           r += 1;  }
    return r;
}

static inline uint32_t fastHash(const uint16_t* p, int len){
    uint32_t h = 2166136261u;
    for(int i=0;i<len;i++){
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

// Hash invertido
static inline uint32_t fastHashRev(const uint16_t* p, int len){
    uint32_t h = 2166136261u;
    for(int i=len-1; i>=0; i--){
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

// Confirmar mirror
static inline int isMirror(const uint16_t* a, const uint16_t* b, int len){
    for(int i=0;i<len;i++){
        if(a[i] != b[len-1-i]) return 0;
    }
    return 1;
}

// Confirmar igual
static inline int isEqual(const uint16_t* a, const uint16_t* b, int len){
    for(int i=0;i<len;i++){
        if(a[i] != b[i]) return 0;
    }
    return 1;
}

//función auxiliar
static inline void readCommand7(u8 byte, int* pInd, u16* surface){
    //determinar cual es el tipo de comando contra el que estamos tratando
    switch((byte>>5) & 0b011){
        case ACSpattern:{
            //000P PPRR
            u8 repeat = (byte & 0b11)+1;
            u8 pixels = ((byte>>2) & 0b111)+2;

            int rInd = *pInd-pixels;
            u8 iterations = repeat*pixels;
            for(int i = 0; i < iterations; i++){
                surface[(*pInd)++] = surface[rInd++];
            }
        break;}
        case ACSmirror:{
            //001x xxxx
            //debemos leer para atrás y escribirlo hacia adelante
            int mInd = *pInd-1;
            u8 repeat = (byte & 0b11111)+2;
            for(int i = 0; i < repeat; i++){
                surface[(*pInd)++] = surface[mInd--];
            }
        break;}

        default:{//repetición simple (01xxxxxx)
            //obtener último color
            u16 col = surface[*pInd-1];
            u8 repeat = (byte & 0b111111)+1;
            for(int i = 0; i<repeat;i++){
                surface[(*pInd)++] = col;
            }
        break;}
    }
}
static inline void readCommand8(u8 byte, int* pInd, u16* surface){
    switch(byte>>6){
        case ACSpattern:{//repeat pattern
            //CCP PPRRR
            u8 repeat = (byte & 0b111)+1;
            u8 pixels = ((byte>>3) & 0b111)+2;

            int rInd = *pInd-pixels;
            u8 iterations = repeat*pixels;
            for(int i = 0; i < iterations; i++){
                surface[(*pInd)++] = surface[rInd++];
            }
        break;}

        case ACSmirror:{//Mirror
            //debemos leer para atrás y escribirlo hacia adelante
            int mInd = *pInd-1;
            u8 repeat = (byte & 0b111111)+2;
            for(int i = 0; i < repeat; i++){
                surface[(*pInd)++] = surface[mInd--];
            }
        break;}

        default:{//repetición simple, caso 10 u 11
            //obtener último indice
            u16 pixel = surface[*pInd-1];
            u8 repeat = (byte & 0b1111111)+1;
            for(int i = 0; i<repeat;i++){
                surface[(*pInd)++] = pixel;
            }
        break;}
    }
}

AcsReader acs;

static inline u16 acsReadU16BE(const u8* p)
{
    return ((u16)p[0] << 8) | p[1];
}

static bool acsLoadFile(const char* path)
{
    acs = AcsReader();   // resetea todo el estado

    FILE* f = fopen(path, "rb");
    if(!f) return false;

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if(size > sizeof(backup)){ fclose(f); return false; }

    u8* buf = (u8*)backup;
    fread(buf, 1, size, f);
    fclose(f);

    acs.data     = buf;
    acs.fileSize = size;
    return true;
}

static bool acsReadHeader()
{
    const u8* d   = acs.data;
    size_t&   pos = acs.headerPos;

    // ---------- byte 0: versión y flags ----------
    acs.hasPalette = (d[0] & ACStypeusesPalette);
    pos = 1;

    // ---------- byte 1: resolución ----------
    static const int resTable[16] = {
        0,4,8,16,24,32,48,64,96,128,192,256,320,512,1024,-1
    };

    u8  val = d[pos++];
    int w   = resTable[val >> 4];
    int h   = resTable[val & 0xF];

    if(w == -1){ w = acsReadU16BE(d + pos); pos += 2; }
    if(h == -1){ h = acsReadU16BE(d + pos); pos += 2; }

    acs.width      = w;
    acs.height     = h;
    acs.pixelCount = (u32)w * (u32)h;

    // ---------- byte 2: bpp + colorMode ----------
    val = d[pos++];
    acs.bppCode      = val >> 6;
    acs.bitsPerPixel = 1 << (acs.bppCode & 0b11);
    acs.colorMode    = (val >> 3) & 0b111;
    if(acs.colorMode > ACStotalModes) return false;

    // ---------- colorCount ----------
    if(acs.hasPalette){
        acs.paletteSize = d[pos++];
    }else{
        if(acs.bitsPerPixel == 16){
            return false; // no hay modo directo sin colores
        }
        acs.paletteSize = 1 << acs.bitsPerPixel;
    }
    return true;
}

static void acsReadPalette(u16* pal)
{
    const u8* d   = acs.data;
    size_t&   pos = acs.headerPos;
    const int count = acs.paletteSize;

    switch(acs.colorMode){

        case ACScolModeARGB1555:
            for(int i = 0; i <= count; i++){
                pal[i] = acsReadU16BE(d + pos);
                pos += 2;
            }
        break;

        case ACScolModeARGB8888:
            for(int i = 0; i <= count; i++){
                u8 a = d[pos++] >> 7;
                u8 r = d[pos++] >> 3;
                u8 g = d[pos++] >> 3;
                u8 b = d[pos++] >> 3;
                pal[i] = (u16)((a<<15)|(r<<10)|(g<<5)|b);
            }
        break;

        case ACScolModeGrayScale4:
            for(int i = 0; i <= count; i++){
                u8 v  = d[pos++];
                u8 hi = (v & 0xF0) >> 3;
                u8 lo = (v & 0x0F) << 1;
                pal[i++] = 0x8000 | (hi<<10) | (hi<<5) | hi;
                if(i >= count) break;
                pal[i]   = 0x8000 | (lo<<10) | (lo<<5) | lo;
            }
        break;

        case ACScolModeGrayScale8:
            for(int i = 0; i <= count; i++){
                u8 v = d[pos++] >> 3;
                pal[i] = 0x8000 | (u16)((v<<10)|(v<<5)|v);
            }
        break;

        case ACScolModeRGB888:
            for(int i = 0; i <= count; i++){
                u8 r = d[pos++] >> 3;
                u8 g = d[pos++] >> 3;
                u8 b = d[pos++] >> 3;
                pal[i] = 0x8000 | (u16)((r<<10)|(g<<5)|b);
            }
        break;
    }
}

static void acsSetupBlocks()
{
    const u8* d   = acs.data;
    size_t&   pos = acs.headerPos;

    acs.ctrlBlockBytes = acsReadU16BE(d + pos); pos += 2;
    acs.cmdBlockBytes  = acsReadU16BE(d + pos); pos += 2;

    acs.ctrlBlock = d + pos;
    acs.cmdBlock  = d + pos + acs.ctrlBlockBytes;
    acs.pixBlock  = d + pos + acs.ctrlBlockBytes + acs.cmdBlockBytes;
    pos += acs.ctrlBlockBytes + acs.cmdBlockBytes;

    acs.ctrlBitPos = 0;
    acs.cmdBytePos = 0;
    acs.pixBytePos = 0;
    acs.outPos     = 0;
    acs.subPixel   = 0;
}

static void acsDecodeIndexed(u16* surface)
{
    const int total = (int)acs.pixelCount;

    switch(acs.bppCode){

        // -------- 8 BPP --------
        case 3:
            while(acs.outPos < total){
                u8  ctrl     = acs.ctrlBlock[acs.ctrlBitPos >> 3];
                int startBit = acs.ctrlBitPos & 7;
                int bitsLeft = 8 - startBit;
                int pixLeft  = total - acs.outPos;
                int n = bitsLeft < pixLeft ? bitsLeft : pixLeft;

                u8 mask = 0x80 >> startBit;
                for(int b = 0; b < n; b++, mask >>= 1){
                    if(ctrl & mask){
                        readCommand8(acs.cmdBlock[acs.cmdBytePos++], &acs.outPos, surface);
                    } else {
                        surface[acs.outPos++] = acs.pixBlock[acs.pixBytePos++];
                    }
                }
                acs.ctrlBitPos += n;
            }
        break;

        // -------- 4 BPP --------
        case 2:
            while(acs.outPos < total){
                u8  ctrl     = acs.ctrlBlock[acs.ctrlBitPos >> 3];
                int startBit = acs.ctrlBitPos & 7;
                int bitsLeft = 8 - startBit;
                int pixLeft  = total - acs.outPos;
                int n = bitsLeft < pixLeft ? bitsLeft : pixLeft;

                u8 mask = 0x80 >> startBit;
                for(int b = 0; b < n; b++, mask >>= 1){
                    if(ctrl & mask){
                        readCommand8(acs.cmdBlock[acs.cmdBytePos++], &acs.outPos, surface);
                    } else {
                        u8 raw = acs.pixBlock[acs.pixBytePos];
                        u8 index;
                        if(acs.subPixel == 0){ index = raw >> 4;   acs.subPixel = 1; }
                        else                 { index = raw & 0x0F; acs.pixBytePos++; acs.subPixel = 0; }
                        surface[acs.outPos++] = index;
                    }
                }
                acs.ctrlBitPos += n;
            }
        break;

        // -------- 2 BPP --------
        case 1:
            while(acs.outPos < total){
                u8  ctrl     = acs.ctrlBlock[acs.ctrlBitPos >> 3];
                int startBit = acs.ctrlBitPos & 7;
                int bitsLeft = 8 - startBit;
                int pixLeft  = total - acs.outPos;
                int n = bitsLeft < pixLeft ? bitsLeft : pixLeft;

                u8 mask = 0x80 >> startBit;
                for(int b = 0; b < n; b++, mask >>= 1){
                    if(ctrl & mask){
                        readCommand8(acs.cmdBlock[acs.cmdBytePos++], &acs.outPos, surface);
                    } else {
                        int shift = 6 - (acs.subPixel << 1);
                        u8 index  = (acs.pixBlock[acs.pixBytePos] >> shift) & 0b11;
                        surface[acs.outPos++] = index;
                        acs.subPixel++;
                        if(shift == 0){ acs.pixBytePos++; acs.subPixel = 0; }
                    }
                }
                acs.ctrlBitPos += n;
            }
        break;

        // -------- 1 BPP --------
        case 0:
            while(acs.outPos < total){
                u8  ctrl     = acs.ctrlBlock[acs.ctrlBitPos >> 3];
                int startBit = acs.ctrlBitPos & 7;
                int bitsLeft = 8 - startBit;
                int pixLeft  = total - acs.outPos;
                int n = bitsLeft < pixLeft ? bitsLeft : pixLeft;

                u8 mask = 0x80 >> startBit;
                for(int b = 0; b < n; b++, mask >>= 1){
                    if(ctrl & mask){
                        readCommand8(acs.cmdBlock[acs.cmdBytePos++], &acs.outPos, surface);
                    } else {
                        int shift = 7 - acs.subPixel;
                        u8 index  = (acs.pixBlock[acs.pixBytePos] >> shift) & 1;
                        surface[acs.outPos++] = index;
                        acs.subPixel++;
                        if(shift == 0){ acs.pixBytePos++; acs.subPixel = 0; }
                    }
                }
                acs.ctrlBitPos += n;
            }
        break;
    }
}

static void acsDecodeDirect(u16* surface)
{
    const u8* d   = acs.data;
    size_t&   pos = acs.headerPos;
    const int total = (int)acs.pixelCount;

    acs.bitsPerPixel = 16;
    acs.outPos = 0;

    switch(acs.colorMode){

        case ACScolModeARGB1555:
            while(acs.outPos < total){
                u8 hi = d[pos++];
                u8 lo = d[pos++];
                if(hi < 0x80){
                    if((hi | lo) != 0){
                        readCommand7(hi, &acs.outPos, surface);
                        pos--;
                        continue;
                    } else {
                        surface[acs.outPos++] = 0;
                        continue;
                    }
                }
                surface[acs.outPos++] = ((u16)hi << 8) | lo;
            }
        break;

        case ACScolModeARGB8888:
            while(acs.outPos < total){
                u8 a = d[pos++] >> 7;
                u8 r = d[pos++] >> 3;
                u8 g = d[pos++] >> 3;
                u8 b = d[pos++] >> 3;
                if(a == 0){
                    if((r | g | b) != 0){
                        readCommand7(r, &acs.outPos, surface);
                        pos -= 2;
                        continue;
                    } else {
                        surface[acs.outPos++] = 0;
                        continue;
                    }
                }
                surface[acs.outPos++] = (u16)((a<<15)|(r<<10)|(g<<5)|b);
            }
        break;

        case ACScolModeRGB888:
            // sin implementar
        break;
    }
}

void importACS(const char* path, u16* surface, u16* pal)
{//este es el cargador para el usuario
    if(!acsLoadFile(path))  return;
    if(!acsReadHeader())    return;

    // ---------- MODO INDEXADO ----------
    if(acs.paletteSize > 0){

        if(acs.hasPalette) acsReadPalette(pal);

        // modo oculto solo-paleta
        if(acs.width == 0 || acs.height == 0) return;

        surf.w = log2(acs.width);
        surf.h = log2(acs.height);
        dmaFillWords(0, surface, 32768);

        acsSetupBlocks();
        acsDecodeIndexed(surface);

    // ---------- MODO DIRECTO ----------
    } else {
        acsDecodeDirect(surface);
    }
    paletteBpp = acs.bitsPerPixel;
}

void importACS16(const char* path, u16* surface, u16* pal)
{//cargar para hardware 16bpp
    if(!acsLoadFile(path))  return;
    if(!acsReadHeader())    return;

    if(acs.paletteSize > 0){

        if(acs.hasPalette) acsReadPalette(pal);

        if(acs.width == 0 || acs.height == 0)
            return;//no debería pasar

        acsSetupBlocks();
        acsDecodeIndexed(surface);
        //convertimos de indexed a direct
        for(int i = 0; i < acs.pixelCount; i++){
            surface[i] = pal[surface[i]];
        }
    } else {
        acsDecodeDirect(surface);
    }
}


void exportACS(const char* path, u16* surface, u16* pal){
    //agregar los otros comandos de exportación

    // revisar si la imagen es válida (solo aplica a esta app)
    if(surf.w < 2 || surf.w >= 8){ return; }
    if(surf.h < 2 || surf.h >= 8){ return; }
    u8* data = (u8*)backup;

    // Byte 0 configuración del archivo
    data[0] = ACStypeusesPalette;

    // resoluciones ACS (reducido específicamente para esta app)
    u8 resTable[8] = {15,15,1,2,3,5,7,9};
    data[1] = (resTable[surf.w] << 4) | (resTable[surf.h]);
    data[2] = 0;// byte configuraciones color/bpp
    data[3] = 0;// byte 3 = cantidad de colores de paleta

    // índice del primer byte libre después del header
    int ind = 4;

    // -------------------------------------------------------------------
    // CONTADOR DE COLORES
    // usamos data[4 ... 32771] como tabla de colores (32768 bytes)
    // -------------------------------------------------------------------
    u8* table = &data[4];

    int totalPixels = (1 << surf.w) * (1 << surf.h);
    int unique = 0;
    int gray   = 1;
    int maxCol = 0;

    // limpiar la tabla de colores sin desbordar
    memset(table, 0, 32768);
    for(int i = 0; i < totalPixels; i++){
        u16 px = surface[i] & 0x7FFF; // ignorar bit alpha ARGB1555

        if(!table[px]){
            table[px] = 1;
            unique++;

            if(px > maxCol) maxCol = px;

            if(gray){
                int r = (px >> 10) & 0x1F;
                int g = (px >>  5) & 0x1F;
                int b =  px        & 0x1F;

                if(!(r == g && g == b)){
                    gray = 0;
                }
            }
        }
    }

    // -------------------------------------------------------------------
    // CONFIG. COLOR Y BPP
    // -------------------------------------------------------------------
    u8 colorConfig = gray ? ACScolModeGrayScale8 : ACScolModeARGB1555;
    u8 bpp = 0; // 1bpp, 2bpp, 4bpp, 8bpp
    if(paletteBpp == 16){
        maxCol = unique;
    }

    if(maxCol < 4){
        bpp = 1;   // 2bpp
    }else if(maxCol < 16){
        bpp = 2;   // 4bpp
    }else if(maxCol < 256){
        bpp = 3;   // 8bpp
    }

    data[2] = (bpp << 6) | (colorConfig << 3);
    //los otros tres bits están reservados

    // BYTE 3: cantidad de colores en paleta
    u8 colorCount = 0;
    if(maxCol > 255){
        // modo directo → no usa paleta
        colorCount = 0;
    }else{
        colorCount = maxCol;
    }
    data[3] = colorCount;

    // PALETA (este editor de pixel art solo exporta a dos formatos de manera nativa)
    if(colorCount > 0){
        if(colorConfig == ACScolModeARGB1555){
            for(int i = 0; i <= colorCount; i++){
                u16 col = pal[i];
                data[ind++] = col >> 8;
                data[ind++] = col & 0xFF;
            }
        }else{
            // Grayscale8
            for(int i = 0; i <= colorCount; i++){
                u8 g = (pal[i] & 0b11111) << 3;
                data[ind++] = g;
            }
        }

    }

    // -------------------------------------------------------------------
    // ESCRITURA DE PÍXELES
    // -------------------------------------------------------------------
    if(colorCount == 0){
        printf("\nUsing direct mode");
        // ====================== MODO DIRECTO ============================
        int iPix = 0;
        u16 lastColor = 0x81;//color inválido en ARGB1555
        const int MAXPAT = 9;
        const int MINPAT = 2; 
        printf("\nLooking for patterns, \nthis will take a time");
        while(iPix < totalPixels){
            u16 curr = surface[iPix];

            // 2) repetición del color (implementado)
            if(curr == lastColor){//comprueba hacia atrás porque el comando copia color
                int run = 1;//un pixel repetido
                while(iPix + run < totalPixels && run < 64){
                    if(surface[iPix+run] == lastColor) run++;
                    else break;
                }
                u8 cmd = (0b01000000 | ((run-1) & 0b111111));//nunca debería ser mayor a 63
                data[ind++] = cmd;

                iPix += run;//sumarle al indice de pixeles
                continue;
            }
            // 3) pixel directo ARGB1555
            data[ind++] = curr >> 8;
            data[ind++] = curr & 0xFF;

            lastColor = curr;
            iPix++;
        }
    }else{
        // ====================== MODO INDEXADO ===========================
        u8* cmdBuf = (u8*)stack;
        u8* pixBuf = (u8*)stack+8192;
        u8* ctrlBuf= (u8*)stack+24576;

        int cmdInd  = 0;
        int ctrlInd = 0;
        int pixInd  = 0;

        int  ctrlBit     = 0;
        u8   currentCtrl = 0;

        int  iPix        = 0;
        const int MAXPAT = 9;
        const int MINPAT = 2;
        const int MINMIRROR = 2;
        const int MAXMIRROR = 65;
        u16 lastRaw   = 0;
        u8  lastIndex = 0;

        while(iPix < totalPixels){

            // cerrar byte-control si está lleno
            if(ctrlBit == 8){
                ctrlBuf[ctrlInd++] = currentCtrl;
                currentCtrl = 0;
                ctrlBit = 0;
            }

            u8 index = surface[iPix]; // surface ya indexada

            int bestSize   = 0;   // tamaño del patrón normal
            int bestRepeat = 0;   // repeticiones encontradas
            int bestMirror = 0;   // tamaño del mirror (independiente)
            int mode       = 0;   // 1 = pattern, 2 = mirror, 0 = nada

            int maxHist = iPix;

            //-----------------------------------------------------
            // 1) DETECCIÓN DE PATRONES NORMALES + REPEATS
            //-----------------------------------------------------
            for(int size = MAXPAT; size >= MINPAT; size--)
            {
                if(size > maxHist) continue;
                if(iPix + size > totalPixels) continue;

                if(!isEqual(&surface[iPix - size], &surface[iPix], size)) continue;

                int rep = 1;
                while(rep < 8){
                    int startB = iPix + rep * size;
                    if(startB + size > totalPixels) break;
                    if(!isEqual(&surface[iPix], &surface[startB], size)) break;
                    rep++;
                }

                bestSize   = size;
                bestRepeat = rep;
                break;
            }

            //-----------------------------------------------------
            // 2) DETECCIÓN DE MIRROR
            //-----------------------------------------------------
            for(int size = MAXMIRROR; size >= MINMIRROR; size--)
            {
                if(size > maxHist) continue;
                if(iPix + size > totalPixels) continue;

                if(isMirror(&surface[iPix], &surface[iPix - size], size)){
                    bestMirror = size;
                    break;
                }
            }

            //-----------------------------------------------------
            // 3) DECISIÓN
            //-----------------------------------------------------
            int savingPattern = (bestSize > 0) ? (bestSize * bestRepeat) - 1 : 0;
            int savingMirror  = (bestMirror > 0) ? bestMirror - 1 : 0;

            if(savingPattern > 1 && savingPattern >= savingMirror) mode = 1;
            else if(savingMirror > 1) mode = 2;

            // --- después de calcular savingPattern, savingMirror ---

            // calcular también ahorro de repetición simple
            int run = 0;
            if(iPix > 0 && index == lastIndex){
                run = 1;
                while(iPix + run < totalPixels && run < 128){
                    if((u8)surface[iPix+run] == lastIndex) run++;
                    else break;
                }
            }
            int savingRun = (run > 0) ? run - 1 : 0;

            // decidir modo final incluyendo run
            if(savingPattern > 1 && savingPattern >= savingMirror && savingPattern >= savingRun) mode = 1;
            else if(savingMirror > 1 && savingMirror >= savingRun) mode = 2;
            else if(run > 0) mode = 3; // repetición simple
            // else mode = 0 → pixel crudo

            if(mode == 1){
                currentCtrl |= (1 << (7 - ctrlBit));
                u8 cmd = (ACSpattern << 6) | ((bestSize - 2) << 3) | (bestRepeat - 1);
                cmdBuf[cmdInd++] = cmd;
                iPix += bestSize * bestRepeat;
                lastIndex = (u8)surface[iPix - 1];
                ctrlBit++; continue;
            }
            else if(mode == 2){
                currentCtrl |= (1 << (7 - ctrlBit));
                u8 cmd = (ACSmirror << 6) | ((bestMirror - 2) & 0b111111);
                cmdBuf[cmdInd++] = cmd;
                iPix += bestMirror;
                lastIndex = (u8)surface[iPix - 1];
                ctrlBit++; continue;
            }
            else if(mode == 3){
                currentCtrl |= (1 << (7 - ctrlBit));
                u8 cmd = 0b10000000 | (run - 1);
                cmdBuf[cmdInd++] = cmd;
                iPix += run;
                ctrlBit++; continue;
            }
            // 3) pixel crudo
            // bit de control = 0
            pixBuf[pixInd++] = index;

            lastRaw   = index;
            lastIndex = index;
            iPix++;
            ctrlBit++;
        }

        // guardar último byte-control si quedó incompleto
        if(ctrlBit > 0){
            ctrlBuf[ctrlInd++] = currentCtrl;
        }

        // header de conteos
        data[ind++] = (ctrlInd >> 8)  & 0xFF;
        data[ind++] = (ctrlInd      ) & 0xFF;

        data[ind++] = (cmdInd  >> 8)  & 0xFF;
        data[ind++] = (cmdInd       ) & 0xFF;
        //el lector no pide cantidad de pixeles

        // 1) byte-controls
        for(int k = 0; k < ctrlInd; k++){
            data[ind++] = ctrlBuf[k];
        }
        printf("\n%d byte controls",ctrlInd);

        // 2) comandos
        for(int k = 0; k < cmdInd; k++){
            data[ind++] = cmdBuf[k];
        }
        printf("\n%d Commands",cmdInd);

        // 3) pixeles en index
        switch(bpp){

            case 0: {//1bpp
                int bitPos = 7;
                u8 byte = 0;

                for(int k = 0; k < pixInd; k++){
                    byte |= (pixBuf[k] & 1) << bitPos;
                    if(--bitPos < 0){
                        data[ind++] = byte;
                        bitPos = 7;
                        byte = 0;
                    }
                }
                if(bitPos != 7)
                    data[ind++] = byte;
            } break;

            case 1: {//2bpp
                int bitPos = 6; // 2 bits por pixel
                u8 byte = 0;

                for(int k = 0; k < pixInd; k++){
                    byte |= (pixBuf[k] & 3) << bitPos;
                    bitPos -= 2;

                    if(bitPos < 0){
                        data[ind++] = byte;
                        bitPos = 6;
                        byte = 0;
                    }
                }
                if(bitPos != 6)
                    data[ind++] = byte;
            } break;

            case 2: {//4bpp
                int high = 1;
                u8 byte = 0;

                for(int k = 0; k < pixInd; k++){
                    if(high){
                        byte = (pixBuf[k] & 0xF) << 4;
                        high = 0;
                    } else {
                        byte |= (pixBuf[k] & 0xF);
                        data[ind++] = byte;//escribir byte
                        high = 1;
                        byte = 0;
                    }
                }
                if(!high)
                    data[ind++] = byte;//terminar de escribir byte
            } break;

            case 3: // 8bpp
                for(int k = 0; k < pixInd; k++){
                    data[ind++] = pixBuf[k];
                }
            break;
        }
        printf("\n%d indexed bytes",pixInd);
    }
    
    int finalSize = ind;
    printf("\nProcess finished\n%d bytes.",finalSize);
    FILE* f = fopen(path, "wb");
    if(!f){
        return;
    }

    fwrite(data, 1, finalSize, f);
    fclose(f);
}

void exportACSpal(const char* path, u16* pal){
    if(paletteBpp > 8){
        return;
    }
    u8* data = (u8*)backup;

    // index of the first byte to write
    int ind = 4;
    int gray= 1;

    for(int i = 0; i < 256; i++){
        u8 r = palette[i] & 31;
        u8 g = (palette[i]>>5) & 31;
        u8 b = (palette[i]>>10) & 31;
        if(r != g && g != b){
            gray = 0;
        }   
    }
    u16 colorCount = (1<<paletteBpp)-1;//placeholder
    u8 colorConfig = gray ? ACScolModeGrayScale8 : ACScolModeARGB1555;
    u8 bpp = 0; // 1bpp, 2bpp, 4bpp, 8bpp

    if(colorCount < 4){
        bpp = 1;   // 2bpp
    }else if(colorCount < 16){
        bpp = 2;   // 4bpp
    }else if(colorCount < 256){
        bpp = 3;   // 8bpp
    }
    // PALETA
    if(colorCount > 0){
        if(colorConfig == ACScolModeARGB1555){
            for(int i = 0; i <= colorCount; i++){
                u16 col = pal[i];
                data[ind++] = col >> 8;
                data[ind++] = col & 0xFF;
            }
        }else{
            // Grayscale8
            for(int i = 0; i <= colorCount; i++){
                u8 g = (pal[i] & 0b11111) << 3;
                data[ind++] = g;
            }
        }
    }
    data[0] = ACStypeusesPalette;
    data[1] = 0;//we don't have an image
    data[2] = (bpp << 6) | (colorConfig << 3);
    data[3] = colorCount;

    int finalSize = ind;
    FILE* f = fopen(path, "wb");
    if(!f){
        return;
    }

    fwrite(data, 1, finalSize, f);
    fclose(f);
}

void exportACSnoPal(const char* path, u16* surface){
    if(paletteBpp > 8){return;}
    if(surf.w < 2 || surf.w >= 8){ return; }
    if(surf.h < 2 || surf.h >= 8){ return; }
    u8* data = (u8*)backup;

    // Byte 0
    data[0] = 0;//don't use palette nor animation

    // resolutions
    u8 resTable[8] = {15,15,1,2,3,5,7,9};
    data[1] = (resTable[surf.w] << 4) | (resTable[surf.h]);

    // índice del primer byte libre después del header
    int ind = 3;

    int totalPixels = (1<<surf.w<<surf.h);

    u8 bpp = 0;
    u8 maxCol = (1<<paletteBpp)-1;
    if(maxCol < 4){
        bpp = 1;   // 2bpp
    }else if(maxCol < 16){
        bpp = 2;   // 4bpp
    }else if(maxCol < 256){
        bpp = 3;   // 8bpp
    }

    data[2] = (bpp << 6);//color config no se usa aquí

    // ====================== MODO INDEXADO ===========================
    u8* cmdBuf = (u8*)stack;
    u8* pixBuf = (u8*)stack+8192;
    u8* ctrlBuf= (u8*)stack+24576;

    int cmdInd  = 0;
    int ctrlInd = 0;
    int pixInd  = 0;

    int  ctrlBit     = 0;
    u8   currentCtrl = 0;

    int  iPix        = 0;
    const int MAXPAT = 9;
    const int MINPAT = 2;
    const int MINMIRROR = 2;
    const int MAXMIRROR = 65;
    u16 lastRaw   = 0;
    u8  lastIndex = 0;

    while(iPix < totalPixels){
        if(ctrlBit == 8){
            ctrlBuf[ctrlInd++] = currentCtrl;
            currentCtrl = 0;
            ctrlBit = 0;
        }
        u8 index = surface[iPix];
        int bestSize = 0, bestRepeat = 0, bestMirror = 0, mode = 0;
        int maxHist = iPix;

        for(int size = MAXPAT; size >= MINPAT; size--)
        {
            if(size > maxHist) continue;
            if(iPix + size > totalPixels) continue;

            if(!isEqual(&surface[iPix - size], &surface[iPix], size)) continue;

            int rep = 1;
            while(rep < 8){
                int startB = iPix + rep * size;
                if(startB + size > totalPixels) break;
                if(!isEqual(&surface[iPix], &surface[startB], size)) break;
                rep++;
            }

            bestSize   = size;
            bestRepeat = rep;
            break;
        }

        for(int size = MAXMIRROR; size >= MINMIRROR; size--)
        {
            if(size > maxHist) continue;
            if(iPix + size > totalPixels) continue;

            if(isMirror(&surface[iPix], &surface[iPix - size], size)){
                bestMirror = size;
                break;
            }
        }

        int savingPattern = (bestSize > 0) ? (bestSize * bestRepeat) - 1 : 0;
        int savingMirror  = (bestMirror > 0) ? bestMirror - 1 : 0;

        if(savingPattern > 1 && savingPattern >= savingMirror) mode = 1;
        else if(savingMirror > 1) mode = 2;

        int run = 0;
        if(iPix > 0 && index == lastIndex){
        run = 1;
        while(iPix + run < totalPixels && run < 128){
                if((u8)surface[iPix+run] == lastIndex) run++;
                else break;
            }
        }
        int savingRun = (run > 0) ? run - 1 : 0;

        if(savingPattern > 1 && savingPattern >= savingMirror && savingPattern >= savingRun) mode = 1;
        else if(savingMirror > 1 && savingMirror >= savingRun) mode = 2;
        else if(run > 0) mode = 3;

        if(mode == 1){
            currentCtrl |= (1 << (7 - ctrlBit));
            u8 cmd = (ACSpattern << 6) | ((bestSize - 2) << 3) | (bestRepeat - 1);
            cmdBuf[cmdInd++] = cmd;
            iPix += bestSize * bestRepeat;
            lastIndex = (u8)surface[iPix - 1];
            ctrlBit++; continue;
        }
        else if(mode == 2){
            currentCtrl |= (1 << (7 - ctrlBit));
            u8 cmd = (ACSmirror << 6) | ((bestMirror - 2) & 0b111111);
            cmdBuf[cmdInd++] = cmd;
            iPix += bestMirror;
            lastIndex = (u8)surface[iPix - 1];
            ctrlBit++; continue;
        }
        else if(mode == 3){
            currentCtrl |= (1 << (7 - ctrlBit));
            u8 cmd = 0b10000000 | (run - 1);
            cmdBuf[cmdInd++] = cmd;
            iPix += run;
            ctrlBit++; continue;
        }
        pixBuf[pixInd++] = index;

        lastRaw   = index;
        lastIndex = index;
        iPix++;
        ctrlBit++;
    }

    if(ctrlBit > 0){
        ctrlBuf[ctrlInd++] = currentCtrl;
    }
    data[ind++] = (ctrlInd >> 8)  & 0xFF;
    data[ind++] = (ctrlInd      ) & 0xFF;

    data[ind++] = (cmdInd  >> 8)  & 0xFF;
    data[ind++] = (cmdInd       ) & 0xFF;

    for(int k = 0; k < ctrlInd; k++){
        data[ind++] = ctrlBuf[k];
    }
    printf("\n%d byte controls",ctrlInd);

    for(int k = 0; k < cmdInd; k++){
        data[ind++] = cmdBuf[k];
    }
    switch(bpp){

        case 0: {//1bpp
            int bitPos = 7;
            u8 byte = 0;

            for(int k = 0; k < pixInd; k++){
                byte |= (pixBuf[k] & 1) << bitPos;
                if(--bitPos < 0){
                    data[ind++] = byte;
                    bitPos = 7;
                    byte = 0;
                }
            }
            if(bitPos != 7)
                data[ind++] = byte;
        } break;

        case 1: {//2bpp
            int bitPos = 6;
            u8 byte = 0;

            for(int k = 0; k < pixInd; k++){
                byte |= (pixBuf[k] & 3) << bitPos;
                bitPos -= 2;
                if(bitPos < 0){
                    data[ind++] = byte;
                    bitPos = 6;
                    byte = 0;
                }
            }
            if(bitPos != 6)
                data[ind++] = byte;
        } break;

        case 2: {//4bpp
            int high = 1;
            u8 byte = 0;

            for(int k = 0; k < pixInd; k++){
                if(high){
                    byte = (pixBuf[k] & 0xF) << 4;
                    high = 0;
                } else {
                    byte |= (pixBuf[k] & 0xF);
                    data[ind++] = byte;//escribir byte
                    high = 1;
                    byte = 0;
                }
            }
            if(!high)
                data[ind++] = byte;//terminar de escribir byte
        } break;

        case 3: // 8bpp
            for(int k = 0; k < pixInd; k++){
                data[ind++] = pixBuf[k];
            }
        break;
    }
    
    int finalSize = ind;
    FILE* f = fopen(path, "wb");
    if(!f){
        return;
    }

    fwrite(data, 1, finalSize, f);
    fclose(f);
}