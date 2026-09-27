#pragma once
#include <nds.h>
#include <maxmod9.h>

extern bool wavPlaying;
extern char musicPath[257];
void initAudio();

mm_word streamingCallback(mm_word length, mm_addr dest, mm_stream_formats format);
void wavStreamFillBuffer(bool forceFill);
bool wavPlay(const char* path);
void wavStreamUpdate();
void wavStop();
void wavContinue();