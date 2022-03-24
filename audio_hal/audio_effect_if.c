#include <string.h>
#include <dlfcn.h>
#include <hardware/hardware.h>
#include <hardware/audio.h>
#include <pthread.h>
#include <system/audio.h>
#include <cutils/log.h>

#include "audio_effect_if.h"

#define EFFECT_FACTORY_LIB "libeffectfactory.so"
static audio_effect_t audio_effect;

int audio_effect_load_interface(audio_hw_device_t *dev, audio_effect_t **effect)
{
    int r;
    void *handle = NULL;
    int (*load)(audio_hw_device_t *device);

    handle = dlopen(EFFECT_FACTORY_LIB, RTLD_NOW);
    if (!handle) {
        const char *err_str = dlerror();
        ALOGE("load: module=%s\n%s", EFFECT_FACTORY_LIB, err_str?err_str:"unknown");
        r = -1;
        goto out;
    }

    load = dlsym(handle, "EffectLoad");
    audio_effect.get_parameters = dlsym(handle, "EffectGetParameters");
    audio_effect.set_parameters = dlsym(handle, "EffectSetParameters");
    if (!load || !audio_effect.get_parameters || !audio_effect.set_parameters) {
        ALOGE("%s: find symbol error", EFFECT_FACTORY_LIB);
        r = -1;
        goto out;
    }
    r = load(dev);
    if (r) {
        ALOGE("call %s fail %s", "EffectLoad");
        r = -1;
        goto out;
    }
    audio_effect.handle = handle;

    *effect = &audio_effect;
    return 0;

out:
    if (handle) {
        dlclose(handle);
        audio_effect.handle = NULL;
    }
    *effect = NULL;
    return r;
}

void audio_effect_unload_interface(audio_hw_device_t *dev)
{
    int (*unload)(audio_hw_device_t *device);

    if (audio_effect.handle) {
        unload = dlsym(audio_effect.handle, "EffectUnload");
        if (!unload) {
            ALOGE("unload: couldn't find symbol %s", "EffectUnload");
            goto out;
        }
        unload(dev);
    }

out:
    if (audio_effect.handle) {
        dlclose(audio_effect.handle);
        audio_effect.handle = NULL;
    }
}

