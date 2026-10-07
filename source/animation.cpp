#include "animation.h"
#include "formatsglobals.h"
#include "timers.h"
#include <unistd.h>
#include "music.h"

#define PALETTE_SIZE (256 * 2)

bool audioSync;
extern bool imgChanges;
static FILE *animationFile = NULL;
static int currentFramePos = -1;

extern void drawInfo();
extern void drawSurfaceBottom();
extern void drawSurfaceMain();
extern int  updatePal(int increment, int *palettePos);
extern void drawColorPalette();
extern void drawAnimationPos();
extern u16 onionSkin;
extern int palettePos;
extern u16 pixelsTop;
extern u16 *pixelsTopVRAM;
extern u16 *pixelsVRAM;
extern bool accurate;
extern u32 frameStartTime;
extern u32 frameEndTime;
extern bool onionSkinEnable;
Animation animation;

size_t extraRamSize = 0;
bool hasExtraRam;
DTCM_DATA bool enableSDcardCache;
void *extraRamBuffer;

#ifdef DSiMode
bool DSiRam = 0;
#endif

void initAnimation(){
    //básicamente aquí asignamos la memoria extra
    hasExtraRam = false;enableSDcardCache = true;
    size_t size = 0;
    
    if(peripheralSlot2InitDefault()){
        extraRamBuffer = peripheralSlot2RamStart();
        extraRamSize = peripheralSlot2RamSize();
        sysSetCartOwner(true);
        if(extraRamBuffer != NULL && extraRamSize > 0){
            hasExtraRam = true;
            enableSDcardCache = false;
            return;
        }
    }
    #ifdef DSiMode
    if(isDSiMode()){
        size = 12579840;//número más cercano a 12mb en el que caben frames sin pasarse.
        extraRamBuffer = malloc(size);
        if(extraRamBuffer != NULL){
            DSiRam = true;
            hasExtraRam = true;
            enableSDcardCache = false;
            extraRamSize = size;
        }
        return;
    }
    #endif
}
void enableSDcache(){
    FILE *f = fopen(ANIM_TEMP, "wb");
    if (!f)
        return;

    const long fileSize = ((2<<surf.w<<surf.h)+PALETTE_SIZE)*animation.frames;
    fwrite(extraRamBuffer, 1, fileSize, f);
    fclose(f);

    #ifdef DSiMode
    if(DSiRam){
        free(extraRamBuffer);
    }
    #endif
    enableSDcardCache = true;
    extraRamSize = 0;
}
void loadAnimFrame(u16 *surface){
    if(animation.pos > animation.frames){
        return;
    }
    if (!animation.isPlaying && onionSkinEnable == true) {
        u16 *dst = &onionSkin;
        u16 *src = &pixelsTop;

        memcpy(dst, src, 128*128*2);
    }

    const u32 screenSize = 2<<surf.w<<surf.h;
    const u32 blkSize = screenSize + PALETTE_SIZE;
    const long offset = animation.pos*blkSize;
    
    if(enableSDcardCache){
        if(!animation.isPlaying){
            animationFile = fopen(ANIM_TEMP, "rb");
            if (!animationFile)
                return;
            fseek(animationFile, offset, SEEK_SET);
        }
        else{
            if(animation.pos == 0){
                fseek(animationFile,0,SEEK_SET);
            }
        }
        
        fread(surface, 1, screenSize, animationFile);      // píxeles
        fread(palette, 1, PALETTE_SIZE, animationFile); // paleta del frame

        if(!animation.isPlaying)
            fclose(animationFile);
    }else{
        dmaCopy(extraRamBuffer+offset, surface, screenSize);
        memcpy(palette,extraRamBuffer+offset+screenSize, PALETTE_SIZE);
    }
}

void saveAnimFrame()
{
    const u32 screenSize = 2<<surf.w<<surf.h;
    const u32 blkSize = screenSize + PALETTE_SIZE;
    const long offset = animation.pos*blkSize;

    if(enableSDcardCache){
        FILE *f = fopen(ANIM_TEMP, "r+b");
        if (!f)
            f = fopen(ANIM_TEMP, "wb");
        if (!f)
            return;

        fseek(f, offset, SEEK_SET);
        fwrite(surface, 1, screenSize, f);
        fwrite(palette, 1, PALETTE_SIZE, f);
        fclose(f);
    }else{
        //primero vemos si hay espacio suficiente en RAM
        if(offset+blkSize > extraRamSize){
            enableSDcache();
        }else{
            //hay espacio, copiamos sin problema
            memcpy(extraRamBuffer+offset,surface,screenSize);
            memcpy(extraRamBuffer+offset+screenSize, palette, PALETTE_SIZE);
        }
    }
}

void nextAnimFrame()
{
    if(imgChanges){
        saveAnimFrame();
    }
    if (animation.pos >= animation.frames)
    {
        animation.pos = 0;
    }
    else
    {
        animation.pos++;
    }
    loadAnimFrame(surface);
    drawColorPalette();
    updatePal(0, &palettePos);
    drawSurfaceMain();
    accurate = true;
    imgChanges = false;
}

void prevAnimFrame(){
    saveAnimFrame();
    if (animation.pos <= 0)
    {
        animation.pos = animation.frames;
    }
    else
    {
        animation.pos--;
    }
    loadAnimFrame(surface);
    drawColorPalette();
    updatePal(0, &palettePos);
    drawSurfaceMain();
    drawSurfaceMain();
    accurate = true;
}

void deleteAnimFrame()
{
    //primero revisemos si hay frames que eliminar
    if(animation.frames <= 0)
        return;
    //esto va a eliminar el último frame
    animation.pos = animation.frames;

    if(enableSDcardCache){
        /*
        Consideré ni poner esta parte para que la SD tenga
        aún menos cambios, pero preferí dejarlo así por si acaso
        */
        FILE *f = fopen(ANIM_TEMP, "rb");
        if (!f)
            return;

        int fd = fileno(f);

        const int blkSize = (2 << surf.w << surf.h) + PALETTE_SIZE;

        ftruncate(fd, ((long)animation.pos * blkSize)-1);
        fclose(f);
    }
    //limpiar con ceros? nah, no gastemos más energía lol
    animation.frames--;
    animation.pos = animation.frames;
    loadAnimFrame(surface);
    drawColorPalette();
    updatePal(0, &palettePos);
    drawSurfaceMain();
    accurate = true;
}

void insertAnimFrame()
{
    //guardamos el frame actual
    saveAnimFrame();
    //ahora saltamos al final
    animation.frames++; 
    animation.pos = animation.frames;

    //nuevo frame (no limpio la paleta porque es más cómodo así para el usuario)
    dmaFillHalfWords(0, surface, surfaceSize*2);
    //guardo para asegurarme de que no desaparezca de manera misteriosa
    saveAnimFrame();
    drawSurfaceMain();
}

void playAnimation()//solo hace un preview de la animación
{
    int animPos = animation.pos;
    if (animation.frames < 1)
        return;

    //antes de reproducir la animación debemos guardar el frame actual
    saveAnimFrame();
    if(audioSync){
        wavStop();
        wavContinue();
    }
    int pixSize = 2 << surf.w << surf.h;
    int sw = 1 << surf.w;
    int sh = 1 << surf.h;

    //guardar la paleta en algún lado
    u16 palcpy[256];
    for(int i = 0; i < 256; i++){
        palcpy[i] =  palette[i];
    }
    if(enableSDcardCache){
        animationFile = fopen(ANIM_TEMP, "rb");
        if (!animationFile)
            return;
    }
    while(animation.isPlaying)
    {
        animation.pos++;
        if (animation.pos > animation.frames)
            animation.pos = 0;
        drawAnimationPos();
        if(paletteBpp != 16){
            loadAnimFrame(stack);//cargamos antes para tener tiempo
        }
        for (int i = 0; i < animation.speed; i++)
        {//mini loop interno; esperar los frames
            scanKeys();
            if (keysDown()){
                animation.isPlaying = false;
                animation.pos = animPos;
                drawSurfaceMain();
                //recuperar paleta
                for(int i = 0; i < 256; i++){
                    palette[i] = palcpy[i];
                }
                drawAnimationPos();//recuperar posición
                return;
            }
            wavStreamUpdate();
            #ifdef DEBUG_CPU
            timerStop();
            swiWaitForVBlank();
            timerContinue();
            #else
            swiWaitForVBlank();
            #endif
        }
        if (paletteBpp == 16)
        {
            loadAnimFrame(pixelsTopVRAM);
        }
        else
        {
            // palette ya fue actualizado por loadAnimFrame
            for (int y = 0; y < sh; y++)
            {
                u16 *dst = pixelsTopVRAM + (y << 7);
                u16 *src = stack + (y << surf.w);
                for (int x = 0; x < sw; x++)
                {
                    dst[x] = palette[src[x]];
                }
            }
        }

        #ifdef DEBUG_CPU
        frameEndTime = timerRead();
        drawInfo();
        timerReset();
        frameStartTime = timerRead();
        #endif
    }
    if(enableSDcardCache){
        fclose(animationFile);
    }
}