#ifndef __AUDIO_EFFECT_IF_H_
#define __AUDIO_EFFECT_IF_H_

#include <hardware/hardware.h>
#include <hardware/audio.h>

#ifdef __cplusplus
extern "C"
{
#endif

//-----------Balance typedef-------------------------------
//Warning:balance is used for 51 bands

#define BALANCE_MAX_BANDS 51

#define BITS    16
#define MAXbit  (1 << (BITS - 1))
#define INT2FLOAT(a) ((float)a / MAXbit)
#define FLOAT2INT(a) (int)((float)a * MAXbit)

typedef enum {
    BALANCE_PARAM_LEVEL = 0,
    BALANCE_PARAM_ENABLE,
    BALANCE_PARAM_LEVEL_NUM,
    BALANCE_PARAM_INDEX,
} Balanceparams;

typedef struct aml_balance_param_s {
    effect_param_t param;
    uint32_t command;
    union {
        int32_t v;
        float f;
        float index[BALANCE_MAX_BANDS];
    };
} aml_balance_param_t;

//-------------VirtualSurround typedef--------------------------
typedef enum {
    VIRTUALSURROUND_PARAM_ENABLE,
    VIRTUALSURROUND_PARAM_EFFECTLEVEL,
} Virtualsurroundparams;

typedef struct aml_virtualsurround_param_s {
    effect_param_t param;
    uint32_t command;
    union {
        int32_t v;
        float f;
    };
} aml_virtualsurround_param_t;

//-------------TrebleBass typedef--------------------------
typedef enum {
    TREBASS_PARAM_INVALID = -1,
    TREBASS_PARAM_BASS_LEVEL,
    TREBASS_PARAM_TREBLE_LEVEL,
    TREBASS_PARAM_ENABLE,
} TREBASSparams;

typedef struct aml_treblebass_param_s {
    effect_param_t param;
    uint32_t command;
    union {
        uint32_t v;
        float f;
    };
} aml_treblebass_param_t;

//-------------Hpeq typedef--------------------------
typedef enum {
    HPEQ_PARAM_INVALID = -1,
    HPEQ_PARAM_ENABLE,
    HPEQ_PARAM_EFFECT_MODE,
    HPEQ_PARAM_EFFECT_CUSTOM,
} HPEQparams;

typedef struct aml_hpeq_param_s {
    effect_param_t param;
    uint32_t command;
    union {
        uint32_t v;
        float f;
        signed char band[5];
    };
} aml_hpeq_param_t;

//-------------Avl typedef--------------------------
typedef enum {
    AVL_PARAM_ENABLE,
    AVL_PARAM_PEAK_LEVEL,
    AVL_PARAM_DYNAMIC_THRESHOLD,
    AVL_PARAM_NOISE_THRESHOLD,
    AVL_PARAM_RESPONSE_TIME,
    AVL_PARAM_RELEASE_TIME,
    AVL_PARAM_SOURCE_IN,
} Avlparams;

typedef struct aml_avl_param_s {
    effect_param_t param;
    uint32_t command;
    union {
        int32_t v;
        float f;
    };
} aml_avl_param_t;

//-------------Effect type typedef--------------------------
typedef enum AML_AUDIO_EFFECT_TYPE {
    AML_EFFECT_BALANCE = 0,
    AML_EFFECT_VIRTUALSURROUND,
    AML_EFFECT_TREBLEBASS,
    AML_EFFECT_HPEQ,
    AML_EFFECT_AVL,
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
