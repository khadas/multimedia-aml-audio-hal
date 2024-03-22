#ifndef __AUDIO_EFFECT_IF_H_
#define __AUDIO_EFFECT_IF_H_

#include <hardware/hardware.h>
#include <hardware/audio.h>

#ifdef __cplusplus
extern "C"
{
#endif

//-------------Effect type typedef--------------------------
typedef enum AML_AUDIO_EFFECT_TYPE {
    AML_EFFECT_BALANCE = 0,
    AML_EFFECT_VIRTUALSURROUND,
    AML_EFFECT_TREBLEBASS,
    AML_EFFECT_HPEQ,
    AML_EFFECT_AVL,
    AML_EFFECT_VIRTUALX,
    AML_EFFECT_VIRTUALX_v4,
    AML_EFFECT_MAX,
} aml_audio_effect_type_e;

typedef struct audio_effect_s {
    void *handle;
    int (*set_parameters)(aml_audio_effect_type_e type,
            uint32_t cmdSize,
            void *pCmdData,
            uint32_t *replySize,
            void *pReplyData);
    int (*get_parameters)(aml_audio_effect_type_e type,
            uint32_t cmdSize,
            void *pCmdData,
            uint32_t *replySize,
            void *pReplyData);
}audio_effect_t;

//-------------Effect API--------------------------
int audio_effect_load_interface(audio_hw_device_t *dev, audio_effect_t **effect);
void audio_effect_unload_interface(audio_hw_device_t *dev);
int audio_effect_set_parameters(aml_audio_effect_type_e type, effect_param_t *param);
int audio_effect_get_parameters(aml_audio_effect_type_e type, effect_param_t *param);

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_EFFECT_IF_H_ */
