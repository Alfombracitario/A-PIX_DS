#include "formatsglobals.h"

#define CAMERA_NDMA_CHANNEL 1
extern u32 kDown;

static int prepareCamera(){
    #ifdef DSiMode
    if(!isDSiMode())
        return -1;
    if(!cameraInit())
        return -1;
    if(!cameraSelect(CAMERA_OUTER))
        return -1;

    return 1;
    #endif
}

int photoTake(){//bastante hardcodeado por ahora
    #ifdef DSiMode
    cameraStopTransfer();
    cameraDeinit();

    surf.w = 7;
    surf.h = 7;
    
    const int VRAM_STRIDE = 256;
    const int Y_OFFSET = 32;
    const int X_START = 32;   // (256-192)/2 para centrar
    const int Y_START = 0;    // Ajusta según la cámara
    
    for(int y = 0; y < 128; y++) {
        for(int x = 0; x < 128; x++) {
            // Mapear píxel de salida a entrada (192x192)
            int src_x = (x * 192) / 128;  // x * 1.5
            int src_y = (y * 192) / 128;  // y * 1.5
            
            int vram_offset = (Y_START + Y_OFFSET + src_y) * VRAM_STRIDE + X_START + src_x;
            int surf_offset = y * 128 + x;
            
            surface[surf_offset] = pixelsTopVRAM[vram_offset];
        }
    }
    return 1;
    #endif
}
int photoPreview(){
    #ifdef DSiMode
    if(!prepareCamera())
        return -1;
    vramSetBankA(VRAM_A_MAIN_BG);
    videoSetMode(MODE_5_2D);
    int bg3Main = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, true, false);

    while(1){
        scanKeys();
        kDown = keysDown();
        if(kDown & KEY_A){
            photoTake();
            return 1;
        }
        else if(kDown & KEY_B){
            break;
        }

        swiWaitForVBlank();
        dmaStopSafe(0);
        if (!ndmaBusy(CAMERA_NDMA_CHANNEL) || !cameraTransferActive())
        {
            cameraStartTransfer(bgGetGfxPtr(bg3Main),
                                MCUREG_APT_SEQ_CMD_PREVIEW,
                                CAMERA_NDMA_CHANNEL);
        }
    }
    cameraStopTransfer();
    cameraDeinit();
    return 1;
    #endif  
}