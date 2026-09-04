#include "formatsglobals.h"

#define CAMERA_NDMA_CHANNEL 1
extern u32 kDown;

static int prepareCamera(){
    if(!isDSiMode())
        return -1;
    if(!cameraInit())
        return -1;
    if(!cameraSelect(CAMERA_OUTER))
        return -1;

    return 1;
}

int photoTake(){
    cameraStopTransfer();
    cameraStartTransfer(backup,
                            MCUREG_APT_SEQ_CMD_PREVIEW,
                            CAMERA_NDMA_CHANNEL);
    while (ndmaBusy(CAMERA_NDMA_CHANNEL))
            swiWaitForVBlank();
            cameraStopTransfer();
    //transformamos el contenido de backup a una resolución menor
    //hardcodeado por ahora
    for(int i = 0; i<(128*96); i++){
        //lo más penca posible por ahora (sí, incluso leyendo basura :>)
        int x = i<<1;
        surface[i] = backup[x-1]+backup[x]+backup[x+255]+backup[x+256]>>2;
    }

    cameraDeinit();
    return 1;
}

int photoPreview(){
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
}