#pragma once
#include "formatsglobals.h"

typedef enum {
    EFFECT_INVERT,
    EFFECT_GRAYSCALE,
    EFFECT_BRIGHTNESS,
    EFFECT_CONTRAST,
    EFFECT_WCROP,
    EFFECT_HCROP,
    EFFECT_WEXPAND,
    EFFECT_HEXPAND,
    EFFECT_TO1BPP,
    EFFECT_TO2BPP,
    EFFECT_TO4BPP,
    EFFECT_TO8BPP,
    EFFECT_TO16BPP,
    EFFECT_POSTERIZE,
    #ifdef DSiMode
    EFFECT_CAMERA,
    EFFECT_VIDEO,
    #endif
    EFFECT_COUNT
} EffectId;

typedef struct {
    const char* name;
    const bool isToggle;     // se aplica sin entrar en parametros
    const int paramMin;
    const int paramMax;
    int paramValue;    // valor actual, editable en vivo
} EffectEntry;

extern EffectEntry effects[EFFECT_COUNT];
extern u8 effectCount;
bool applyEffect(EffectId id);
void posterize(int numColors);