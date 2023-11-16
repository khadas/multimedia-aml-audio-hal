/*
 * Copyright (C) 2023 Amlogic Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define LOG_TAG "aml_audio_aec"

#include <dlfcn.h>
#include <cutils/log.h>
#include <audio_hw_utils.h>
#include "aml_malloc_debug.h"
#include "aml_audio_aec.h"

#ifdef HIFIA
#define AML_FRAME_SIZE 256
#elif ARMV8
#define AML_FRAME_SIZE 256
#else
#define AML_FRAME_SIZE 256
#endif

#define AML_DATA_BIT 16
#define AML_SAMPLE_RATE 16000

typedef struct amlAI_FRONT_PROCESS_CONFIG_S
{
	float f32MicMulValue;            /* (0.0, 10.0) */
	int s32RefDelayMicAlignLength;   /* [0, SampleRate] */
	int s32AdaptDelayAlign;          /* [0, 1] */
	int s32RefreshFreqGap;           /* [1, 30] */
	int s32RefDelayMicRange;         /* [0, SampleRate] */
	int s32Reserved;
} AI_FRONT_PROCESS_CONFIG_S;

typedef struct amlAI_HPF_CONFIG_S
{
	int s32Order;                    /* [1, 3] */
	int s32HpfFreq;                  /* (0, 200] */
	int s32Reserved;
} AI_HPF_CONFIG_S;

typedef struct amlAI_AEC_PF1_CONFIG_S
{
	int s32BandWidth;                /* [64, 256] */
	int s32RefreshLength;            /* [0, 20] */
	int s32Reserved;
} AI_AEC_PF1_CONFIG_S;

typedef struct amlAI_AEC_PF2_CONFIG_S
{
	int s32Intensity;                /* [1, 3] */
	int s32ComfortNoise;             /* [0, 1] */
	int s32Reserved;
} AI_AEC_PF2_CONFIG_S;

typedef struct amlAI_AEC_CONFIG_S
{
	int s32Mode;                     /* [0, 1] */
	int bUsrSingleRefChannelMode;    /* [0, 1] */
	int s32SingleChannelId;          /* [0, MicChannels) */
	int s32EchoPathLength;           /* [0, SampleRate/2] */
	int s32CngMode;                  /* unsupport */
	int bPf1Open;                    /* [0, 1] */
	AI_AEC_PF1_CONFIG_S stAecPf1Cfg;
	int bPf2Open;                    /* [0, 1] */
	AI_AEC_PF2_CONFIG_S stAecPf2Cfg;
	int bPf3Open;                    /* [0, 1] */
	int s32Reserved;
} AI_AEC_CONFIG_S;

typedef struct amlAI_DEREVERBERATION_CONFIG_S
{
	int s32StartPoint;               /* [1, 10] */
	int s32PathLength;               /* [1, 16] */
	int s32Reserved;
} AI_DEREVERBERATION_CONFIG_S;

typedef struct amlAI_ANR_CONFIG_S
{
	int s32Mode;                     /* [0, 1] */
	int s32NrIntensity;              /* [0, 50] */
	int s32Reserved;
} AI_ANR_CONFIG_S;

typedef struct amlAI_GAIN_CONFIG_S
{
	int s32Mode;                     /* [0, 1] */
	int s32FixGainDB;
	int s32AgcGainDB;                /* [0, +oo] */
	int s32AgcGainLevel;
	int s32Reserved;
} AI_GAIN_CONFIG_S;

typedef struct amlAI_NOISE_CONFIG_S
{
	int s32Snr;
	int s32Reserved;
} AI_NOISE_CONFIG_S;

typedef struct amlAI_SPEAKER_CONFIG_S
{
	int s32SampleRate;
	int s32SpeakerChannels;
	int s32FormatDataByte;
	int s32HpfOrder;
	int s32HpfFreq;
	int s32LpfOrder;
	int s32LpfFreq;
	float f32SpeakerMulValue;
	int s32Reserved;
} AI_SPEAKER_CONFIG_S;

typedef struct amlAI_ASP_CONFIG_S
{
	int bHpfOpen;                    /* [0, 1] */
	int bAecOpen;                    /* [0, 1] */
	int bDereverberationOpen;        /* [0, 1] */
	int bAnrOpen;                    /* [0, 1] */
	int bGainOpen;                   /* [0, 1] */
	int bDetecteEnergyOpen;          /* [0, 1] */
	int bNoiseOpen;                  /* [0, 1] */
	int bMisoOpen;                   /* [0, 1] */
	int bSpeakerOpen;                /* [0, 1] */

	int SampleRate;                  /* 8k, 16k */
	int FrameSample;                 /* AML_FRAME_SIZE */
	int FormatDataByte;              /* AML_DATA_BIT >> 3 */

	int MicChannels;
	int RefChannels;

	AI_FRONT_PROCESS_CONFIG_S stFrontCfg;
	AI_HPF_CONFIG_S stHpfCfg;
	AI_AEC_CONFIG_S stAecCfg;
	AI_DEREVERBERATION_CONFIG_S stDereverberationCfg;
	AI_ANR_CONFIG_S stAnrCfg;
	AI_GAIN_CONFIG_S stGainCfg;
	AI_NOISE_CONFIG_S stNoiseCfg;

	AI_SPEAKER_CONFIG_S stSpeakerCfg;
	int s32Reserved;
} AI_ASP_CONFIG_S;


typedef enum ChannelType
{
	MicType = 0,
	RefType
} ChannelType;

typedef struct aml_aec_operations {
    void *(*create)(AI_ASP_CONFIG_S *config);
    int (*import)(void *, void *in, int stride, int channel_id, ChannelType channel_type);
    int (*process)(void *);
    int (*export)(void *, void *out, int stride, int channel_id);
    void (*destroy)(void *);
    void *paec;
}aml_aec_operations_t;

static aml_aec_operations_t aml_aec_ops = {
    .create = NULL,
    .import = NULL,
    .process = NULL,
    .export = NULL,
    .destroy = NULL,
    .paec = NULL,
};

static int unload_aml_aec_lib()
{
    aml_aec_ops.create = NULL;
    aml_aec_ops.import = NULL;
    aml_aec_ops.process = NULL;
    aml_aec_ops.export = NULL;
    aml_aec_ops.destroy = NULL;
    if (aml_aec_ops.paec!= NULL) {
        dlclose(aml_aec_ops.paec);
        aml_aec_ops.paec = NULL;
    }
    return 0;
}
static int load_aml_aec_lib()
{
    aml_aec_ops.paec = dlopen(AML_AEC_LIB_PATH, RTLD_NOW);
    if (aml_aec_ops.paec == NULL) {
        goto err;
    }
    AM_LOGV("loading aec lib.");

    aml_aec_ops.create = (void *(*)(AI_ASP_CONFIG_S *config)) dlsym(aml_aec_ops.paec, "aml_asp_create");
    if (aml_aec_ops.create == NULL) {
        goto err;
    }

    aml_aec_ops.import = (int (*)(void *, void *in, int stride, int channel_id, ChannelType channel_type))
                          dlsym(aml_aec_ops.paec, "aml_asp_import");
    if (aml_aec_ops.import == NULL) {
        goto err;
    }

    aml_aec_ops.process = (int (*)(void *)) dlsym(aml_aec_ops.paec, "aml_asp_process");
    if (aml_aec_ops.process == NULL) {
        goto err;
    }

    aml_aec_ops.export = (int (*)(void *, void *out, int stride, int channel_id))
                            dlsym(aml_aec_ops.paec, "aml_asp_export");
    if (aml_aec_ops.export == NULL) {
        goto err;
    }

    aml_aec_ops.destroy = (void (*)(void *))dlsym(aml_aec_ops.paec, "aml_asp_destroy");
    if (aml_aec_ops.destroy == NULL) {
        goto err;
    }
    return 0;
err:
    if (aml_aec_ops.paec != NULL) {
        dlclose(aml_aec_ops.paec);
    }
    AM_LOGE("loading aec lib fail.");
    return -1;
}
/**
 * \create an instance of aec
 * \param mic_chan: the number of mic channels in the total channel during loopback recording
 * \param pconfig: the configuration parameter when using pcm_open to open the device
 * \return structure containing necessary parameters and instances of aec
 */
struct aec_context* aec_create(int mic_channels, struct pcm_config config)
{
    if (config.rate != 8000 && config.rate != 16000) {
        AM_LOGE("%s aec does not support %d rate\n", __func__, config.rate);
        return NULL;
    }
    AI_ASP_CONFIG_S mConfig;
    int ret = load_aml_aec_lib();
    if (ret != 0) {
        return NULL;
    }

    memset(&mConfig, 0, sizeof(mConfig));
    mConfig.SampleRate = config.rate;                      /* 8k 16k */

    mConfig.FrameSample = AML_FRAME_SIZE;
    mConfig.FormatDataByte = AML_DATA_BIT >> 3;
    mConfig.MicChannels = mic_channels;
    mConfig.RefChannels = config.channels - mic_channels;

    mConfig.stFrontCfg.f32MicMulValue = 1.0;               /* (0.0, 10.0) */
    mConfig.stFrontCfg.s32AdaptDelayAlign = 1;
    mConfig.stFrontCfg.s32RefreshFreqGap = 10;
    mConfig.stFrontCfg.s32RefDelayMicAlignLength = 16000;  /* [0, 4*FrameSample] */
    mConfig.stFrontCfg.s32RefDelayMicRange = 16000;        /* [0, 4*FrameSample] */

    mConfig.bHpfOpen = 0;


    mConfig.bAecOpen = 1;
    if (mConfig.bAecOpen) {
        mConfig.stAecCfg.s32Mode = 2;
        mConfig.stAecCfg.s32EchoPathLength = mConfig.SampleRate / 4;
        mConfig.stAecCfg.bUsrSingleRefChannelMode = 0;
        if (mConfig.stAecCfg.bUsrSingleRefChannelMode)
            mConfig.stAecCfg.s32SingleChannelId = 0;

        mConfig.stAecCfg.bPf2Open = 1;
        if (mConfig.stAecCfg.bPf2Open) {
            mConfig.stAecCfg.stAecPf2Cfg.s32Intensity = 2;
            mConfig.stAecCfg.stAecPf2Cfg.s32ComfortNoise = 1;
        }

        mConfig.stAecCfg.bPf3Open = 0;
        mConfig.stAecCfg.bPf1Open = 0;
    }

    mConfig.bAnrOpen = 1;
    if (mConfig.bAnrOpen) {
        mConfig.stAnrCfg.s32Mode = 1;
        mConfig.stAnrCfg.s32NrIntensity = 1;
        mConfig.stAnrCfg.s32Reserved = 0;
    }

    mConfig.bGainOpen = 0;
    mConfig.bNoiseOpen = 0;
    mConfig.bDetecteEnergyOpen = 0;
    mConfig.bDereverberationOpen = 0;

    struct aec_context* aec = aml_audio_calloc(1, sizeof(struct aec_context));
    aec->config = config;
    aec->mic_channels = mic_channels;
    aec->aml_aec = (*(aml_aec_ops.create))(&mConfig);
    return aec;
}

/**
 * \destroy an instance of aec
 * \param aml_aec: instance of aec
 * \return NULL
 */
void aec_destroy(struct aec_context *aec)
{
    AM_LOGI("[%s:%d], destroy_aec handle: %p", __func__, __LINE__, aec);
    if (aec->aml_aec != NULL)
        aml_aec_ops.destroy(aec->aml_aec);
    aec->aml_aec = NULL;
    aml_audio_free(aec);
    unload_aml_aec_lib();
}

/**
 * \AEC processing of input data
 * \param aec_task: structure pointer containing necessary parameters and instances of aec
 * \param in: input data
 * \param out: output data
 * \return 1 if aec process successful.
 * \return 0 if aec process failed.
 */
int aec_process(struct aec_context* aec, void* in, void* out)
{
    int in_chan = aec->config.channels;
    int out_chan = aec->mic_channels;
    int bytes = pcm_format_to_bits(aec->config.format) >> 3;
    int process_ret = 0;
    if (aec->aml_aec != NULL) {
        int i = 0;
        for (i = 0; i < out_chan; i++)
            aml_aec_ops.import(aec->aml_aec, (void *)((char *)in + i * bytes), in_chan, i, MicType);
        for (i = out_chan; i < in_chan; i++)
            aml_aec_ops.import(aec->aml_aec, (void *)((char *)in + i * bytes), in_chan, i - out_chan, RefType);
        process_ret = aml_aec_ops.process(aec->aml_aec);
        if (process_ret == 0) {
            AM_LOGE("error: no aec process\n");
            return process_ret;
        }
        for (i = 0; i < out_chan; i++)
            aml_aec_ops.export(aec->aml_aec, (void *)((char *)out + i * bytes), out_chan, i);
    } else
        AM_LOGE("aml_aec is NULL\n");
    return process_ret;
}
