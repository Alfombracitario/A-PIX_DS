#include "formatsglobals.h"
#pragma once

typedef enum {
    EFFECT_INVERT,
    EFFECT_GRAYSCALE,
    EFFECT_BRIGHTNESS,
    EFFECT_CONTRAST,
    EFFECT_WCROP,
    EFFECT_HCROP,
    EFFECT_WEXPAND,
    EFFECT_HEXPAND,
    EFFECT_TO16BPP,
    EFFECT_POSTERIZE,
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

bool applyEffect(EffectId id);