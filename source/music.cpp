// parte del código de aquí fue hecho a base de examples de BlocksDS
#include <nds.h>
#include <maxmod9.h>
#include <stdio.h>
#include <string.h>
#include "formatsglobals.h"
#include "music.h"
#include "nds/system.h"
extern int totalBackups;
extern int oldestBackup;
extern int backupIndex;
extern int backupMax;

char musicPath[257];
bool wavPlaying;

// IDs para verificar chunks WAV
#define DATA_ID 0x61746164
#define FMT_ID  0x20746d66
#define RIFF_ID 0x46464952
#define WAVE_ID 0x45564157

typedef struct {
    u32 chunkID;
    u32 chunkSize;
    u32 format;
    u32 subchunk1ID;
    u32 subchunk1Size;
    u16 audioFormat;
    u16 numChannels;
    u32 sampleRate;
    u32 byteRate;
    u16 blockAlign;
    u16 bitsPerSample;
    u32 subchunk2ID;
    u32 subchunk2Size;
} WAVHeader_t;

#define AUDIO_BUFFER_SIZE (256 * 1024)

u8* music;
FILE* wavFile = NULL;

int stream_buffer_in = 0;
int stream_buffer_out = 0;
size_t stream_buffer_available = 0;

bool wavFinished = false;
bool wavStopPending = false;

mm_stream activeStream;

void initAudio(){
    // Dedicar 256kb de RAM a la música
    const int backupSpace = (BACKUP_SIZE*sizeof(u16)) - AUDIO_BUFFER_SIZE;
    music = (u8*)(backup + backupSpace);
    backupMax = backupSpace / surfaceBytes;
    backupIndex = -1;
    oldestBackup = 0;
    totalBackups = 0;
    
    // Inicializar maxmod sin soundbank
    mm_ds_system mmSys = {
        .mod_count    = 0,
        .samp_count   = 0,
        .mem_bank     = 0,
        .fifo_channel = FIFO_MAXMOD
    };
    //Mejorar el audio si estamos en modo DSi
    #ifdef DSiMode
        if(isDSiMode()){
            soundExtSetFrequency(47);
            //soundExtSetRatio(0);
        }
    #endif
    mmInit(&mmSys);
}

// Callback que maxmod llama para obtener datos streameados
mm_word streamingCallback(mm_word length, mm_addr dest, mm_stream_formats format) {
    size_t multiplier = 0;
    
    if (format == MM_STREAM_8BIT_MONO)
        multiplier = 1;
    else if (format == MM_STREAM_8BIT_STEREO)
        multiplier = 2;
    else if (format == MM_STREAM_16BIT_MONO)
        multiplier = 2;
    else if (format == MM_STREAM_16BIT_STEREO)
        multiplier = 4;
    
    size_t size = length * multiplier;

    // Si no queda suficiente información, rellenamos con silencio.
    if (stream_buffer_available < size) {
        size_t available = stream_buffer_available;

        if (available > 0) {
            size_t bytes_until_end = AUDIO_BUFFER_SIZE - stream_buffer_out;

            if (bytes_until_end > available)
                bytes_until_end = available;

            memcpy(dest, &music[stream_buffer_out], bytes_until_end);

            if (available > bytes_until_end) {
                memcpy(
                    (u8*)dest + bytes_until_end,
                    music,
                    available - bytes_until_end
                );
            }

            stream_buffer_out =
                (stream_buffer_out + available) % AUDIO_BUFFER_SIZE;

            stream_buffer_available = 0;
        }

        // El resto del bloque debe ser silencio.
        memset((u8*)dest + available, 0, size - available);

        if (wavFinished)
            wavStopPending = true;

        return length;
    }

    size_t bytes_until_end = AUDIO_BUFFER_SIZE - stream_buffer_out;

    if (bytes_until_end >= size) {
        memcpy(dest, &music[stream_buffer_out], size);
    } else {
        memcpy(dest, &music[stream_buffer_out], bytes_until_end);
        memcpy(
            (u8*)dest + bytes_until_end,
            music,
            size - bytes_until_end
        );
    }

    stream_buffer_out =
        (stream_buffer_out + size) % AUDIO_BUFFER_SIZE;

    stream_buffer_available -= size;

    return length;
}

void wavStreamFillBuffer(bool forceFill) {
    if (!wavFile && wavPlaying)
        return;

    if (!forceFill && wavFinished)
        return;

    // Buffer completamente lleno.
    if (stream_buffer_available >= AUDIO_BUFFER_SIZE)
        return;

    size_t freeSpace = AUDIO_BUFFER_SIZE - stream_buffer_available;

    size_t firstPart = AUDIO_BUFFER_SIZE - stream_buffer_in;

    if (firstPart > freeSpace)
        firstPart = freeSpace;

    size_t bytesRead = fread(
        &music[stream_buffer_in],
        1,
        firstPart,
        wavFile
    );

    stream_buffer_in =
        (stream_buffer_in + bytesRead) % AUDIO_BUFFER_SIZE;

    stream_buffer_available += bytesRead;

    if (bytesRead < firstPart) {
        wavFinished = true;
        return;
    }

    freeSpace -= bytesRead;

    // Si queda espacio, podemos continuar desde el comienzo
    // del buffer circular.
    if (freeSpace > 0) {
        bytesRead = fread(
            music,
            1,
            freeSpace,
            wavFile
        );

        stream_buffer_in = bytesRead;
        stream_buffer_available += bytesRead;

        if (bytesRead < freeSpace)
            wavFinished = true;
    }
}

static int validateWAVHeader(const WAVHeader_t* header) {
    if (header->chunkID != RIFF_ID) return 1;
    if (header->format != WAVE_ID) return 1;
    if (header->subchunk1ID != FMT_ID) return 1;
    if (header->subchunk2ID != DATA_ID) return 1;
    if (header->audioFormat != 1) return 1; // Solo PCM
    return 0;
}

static mm_stream_formats getStreamFormat(
    uint16_t numChannels,
    uint16_t bitsPerSample
) {
    if (numChannels == 1) {
        return (bitsPerSample == 8)
            ? MM_STREAM_8BIT_MONO
            : MM_STREAM_16BIT_MONO;
    } else {
        return (bitsPerSample == 8)
            ? MM_STREAM_8BIT_STEREO
            : MM_STREAM_16BIT_STEREO;
    }
}

// Abre un WAV y comienza a reproducirlo
bool wavPlay(const char* path) {
    strncpy(musicPath, path, 257);
    // Cerrar stream anterior si existe
    if (wavFile) {
        mmStreamClose();
        fclose(wavFile);
        wavFile = NULL;
    }

    wavFile = fopen(path, "rb");

    if (!wavFile)
        return false;

    WAVHeader_t wavHeader;

    if (fread(
        &wavHeader,
        1,
        sizeof(WAVHeader_t),
        wavFile
    ) != sizeof(WAVHeader_t)) {
        fclose(wavFile);
        wavFile = NULL;
        return false;
    }

    if (validateWAVHeader(&wavHeader) != 0) {
        fclose(wavFile);
        wavFile = NULL;
        return false;
    }

    // Resetear buffers
    stream_buffer_in = 0;
    stream_buffer_out = 0;
    stream_buffer_available = 0;

    wavFinished = false;
    wavStopPending = false;

    // Llenar el buffer antes de comenzar
    wavStreamFillBuffer(true);

    // Configurar el stream
    activeStream.sampling_rate = wavHeader.sampleRate;
    activeStream.buffer_length = 2048;
    activeStream.callback = streamingCallback;
    activeStream.format = getStreamFormat(
        wavHeader.numChannels,
        wavHeader.bitsPerSample
    );
    activeStream.timer = MM_TIMER0;
    activeStream.manual = false;

    mmStreamOpen(&activeStream);
    wavPlaying = true;
    disableSleep();
    powerOn(PM_SOUND_AMP);
    return wavPlaying;
}

// Detiene la reproducción manualmente
void wavStop() {
    if (wavFile) {
        mmStreamClose();

        fclose(wavFile);
        wavFile = NULL;
    }
    wavPlaying = false;
    wavStopPending = false;
    wavFinished = false;

    stream_buffer_in = 0;
    stream_buffer_out = 0;
    stream_buffer_available = 0;

    enableSleep();
    powerOff(PM_SOUND_AMP);
}

void wavContinue(){
    wavPlay(musicPath);
}
// Llena el buffer y comprueba si el WAV terminó
ITCM_CODE void wavStreamUpdate() {
    if (!wavFile)
        return;

    if (keysHeld() & KEY_LID) {
        powerOff(POWER_LCD);
        powerOff(PM_SOUND_MUTE);//solo suenan los audífonos
        setCpuClock(false);//downclock
        //Bucle interno para seguir reproduciendo música
        while (keysHeld() & KEY_LID) {
            wavStreamFillBuffer(false);
            swiWaitForVBlank();
            scanKeys();
        }
        //ya se abrió, restauramos
        setCpuClock(true);
        powerOn(PM_SOUND_MUTE);
        powerOn(POWER_LCD);
    }

    wavStreamFillBuffer(false);

    // El callback ya consumió todo el último bloque.
    // Ahora podemos cerrar el stream.
    if (wavStopPending && stream_buffer_available == 0) {
        wavStop();
    }
}

//sé que no es música pero el microfono es sonido así que...
u32 micPointer;
u32 micRecording;

void microphone_handler(void *completed_buffer, int length)
{
    if (!micRecording)
        return;

    if (micRecording >= BACKUP_SIZE)
    {
        micRecording = false;
        return;
    }

    if (micRecording + length > BACKUP_SIZE)
        length = BACKUP_SIZE - micRecording;

    dmaCopy(completed_buffer, backup + micRecording, length);

    micRecording += length;
}

void recordAudio(){
    wavStop();

    
    DC_FlushAll();
    #ifdef DSiMode
    if(isDSiMode()){
        soundMicPowerOn();
    }
    #endif
    soundMicRecord(backup, sizeof(backup),
                MicFormat_12Bit, 16000, microphone_handler);
    
    soundMicOff();
    #ifdef DSiMode
    if(isDSiMode()){
        soundMicPowerOff();
    }
    #endif
}
