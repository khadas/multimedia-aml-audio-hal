/*
 * Copyright (C) 2017 Amlogic Corporation.
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

#define LOG_TAG "audio_hw_decoder_ms12v2"
//#define LOG_NDEBUG 0
//#define LOG_NALOGV 0

#include <cutils/log.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <inttypes.h>

#include "DolbyMS12.h"
#include "DolbyMS12ConfigParams.h"

namespace android
{

//static function pointer
int (*FuncGetMS12OutputMaxSize)(void);
void * (*FuncDolbyMS12Init)(int argc, char **pp_argv);
void (*FuncDolbyMS12Release)(void *);

/*this api will use params to config graph info*/
int (*FuncDolbyMS12InitAllParams)(void *, int argc, char **pp_argv);
/* main Decoder API */
int (*FuncDolbyMs12DecoderOpen)(void *, int argc, char **pp_argv);
int (*FuncDolbyMs12DecoderClose)(void *);
int (*FuncDolbyMs12DecoderProcess)(void *);
int (*FuncDolbyMS12InputMain)(void *, const void *, size_t, int, int, int);
/* main Decoder API End*/

/* ms12 Encoder API */
int (*FuncDolbyMs12EncoderOpen)(void *, int argc, char **pp_argv);
int (*FuncDolbyMs12EncoderClose)(void *);
/* ms12 Encoder API End*/

int (*FuncDolbyMS12InputAssociate)(void *, const void *, size_t, int, int, int);
int (*FuncDolbyMS12InputSystem)(void *, const void *, size_t, int, int, int);
int (*FuncDolbyMS12InputApp)(void *, const void *, size_t, int, int, int);

#ifdef REPLACE_OUTPUT_BUFFER_WITH_CALLBACK
int (*FuncDolbyMS12RegisterOutputCallback)(output_callback , void *);
#else
int (*FuncDolbyMS12Output)(void *, const void *, size_t);
#endif

int (*FuncDolbyMS12RegisterSyncCallback)(void *, ms12sync_callback , void *);


int (*FuncDolbyMS12UpdateRuntimeParams)(void *, int , char **);
int (*FuncDolbyMS12UpdateRuntimeParamsNoLock)(void *, int , char **);
int (*FuncDolbyMS12SchedulerRun)(void *);
void (*FuncDolbyMS12SetQuitFlag)(int);
void (*FuncDolbyMS12FlushInputBuffer)(void);
void (*FuncDolbyMS12FlushMainInputBuffer)(void);
void (*FuncDolbyMS12FlushAppInputBuffer)(void);
unsigned long long (*FuncDolbyMS12GetNBytesConsumed)(void *, int, int);
unsigned long long (*FuncDolbyMS12GetNFramesPCMOutput)(void *, int, int);
unsigned long long (*FuncDolbyMS12GetContinuousNFramesPCMOutput)(void *, int);
void (*FuncDolbyMS12GetPCMOutputSize)(unsigned long long *, unsigned long long *);
void (*FuncDolbyMS12GetBitstreamOutputSize)(unsigned long long *, unsigned long long *);

int (*FuncDolbyMS12GetMainBufferAvail)(int *);
int (*FuncDolbyMS12GetAssociateBufferAvail)(void);
int (*FuncDolbyMS12GetSystemBufferAvail)(int *);

int (*FuncDolbyMS12GetGain)(int);
int (*FuncDolbyMS12Config)(ms12_config_type_t, ms12_config_t *);

int (*FuncDolbyMS12GetAudioInfo)(struct aml_audio_info *);
int (*FuncDolbyMS12GetMATDecLatency)(void);

void (*FuncDolbyMS12SetDebugLevel)(int);
unsigned long long (*FuncDolbyMS12GetNBytesConsumedSysSound)(void);
int (*FuncDolbyMS12GetTotalNFramesDelay)(void *);
int (*FuncDumpDolbyMS12Info)(int);
int (*FuncDolbyMS12HWSyncInit)(void);
int (*FuncDolbyMS12HWSyncRelease)(void);
int (*FuncDolbyMS12HWSyncCheckinPTS)(int offset, int apts);

int (*FuncDolbyMS12SetPtsGap)(unsigned long long offset, int pts_duration);
char * (*FunDolbMS12GetVersion)(void);

/* MAT Encoder API Begin */
int (*FuncDolbyMS12MATEncoderInit)(int, int, unsigned int *, int, int, void **);
int (*FuncDolbyMS12MATEncoderCleanup)(void *);
int (*FuncDolbyMS12MATEncoderProcess)(void *, const unsigned char *, int, const unsigned char *, int *, int, int *);
int (*FuncDolbyMS12MATEncoderConfig)(void *, mat_enc_config_type_t, mat_enc_config_t *);
/* MAT Encoder API End */

int (*FuncDolbyMS12RegisterCallbackbyType)(int, void*, void *);

DolbyMS12::DolbyMS12() :
    mDolbyMS12LibHandle(NULL)
{
    ALOGD("%s()", __FUNCTION__);
}


DolbyMS12::~DolbyMS12()
{
    ALOGD("%s()", __FUNCTION__);
}


int DolbyMS12::GetLibHandle(char *dolby_ms12_path)
{
    ALOGD("+%s()", __FUNCTION__);
    //ReleaseLibHandle();
    if (mDolbyMS12LibHandle) {
        ALOGI("%s lib exists", __func__);
        return 0;
    }
    //here there are two paths, "the DOLBY_MS12_LIB_PATH_A/B", where could exit that dolby ms12 library.
    mDolbyMS12LibHandle = dlopen(dolby_ms12_path, RTLD_NOW);
    if (!mDolbyMS12LibHandle) {
        ALOGE("%s, failed to load libdolbyms12 lib %s\n", __FUNCTION__, dlerror());
        goto ERROR;
    }

    FuncGetMS12OutputMaxSize = (int (*)(void)) dlsym(mDolbyMS12LibHandle, "get_ms12_output_max_size");
    if (!FuncGetMS12OutputMaxSize) {
        ALOGE("%s, dlsym get_ms12_output_max_size fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12Init = (void* (*)(int, char **)) dlsym(mDolbyMS12LibHandle, "ms12_init");
    if (!FuncDolbyMS12Init) {
        ALOGE("%s, dlsym ms12_init fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12Release = (void (*)(void *)) dlsym(mDolbyMS12LibHandle, "ms12_release");
    if (!FuncDolbyMS12Release) {
        ALOGE("%s, dlsym ms12_release fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12InitAllParams = (int (*)(void *, int argc, char **pp_argv)) dlsym(mDolbyMS12LibHandle, "ms12_init_all_params");
    if (!FuncDolbyMS12InitAllParams) {
        ALOGE("%s, dlsym ms12_init_all_params fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMs12DecoderOpen = (int (*)(void *, int argc, char **pp_argv)) dlsym(mDolbyMS12LibHandle, "ms12_decoder_open");
    if (!FuncDolbyMs12DecoderOpen) {
        ALOGE("%s, dlsym ms12_decoder_open fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMs12DecoderClose = (int (*)(void *)) dlsym(mDolbyMS12LibHandle, "ms12_decoder_close");
    if (!FuncDolbyMs12DecoderClose) {
        ALOGE("%s, dlsym ms12_decoder_close fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMs12DecoderProcess = (int (*)(void *)) dlsym(mDolbyMS12LibHandle, "ms12_decoder_process");
    if (!FuncDolbyMs12DecoderProcess) {
        ALOGE("%s, dlsym ms12_decoder_process fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12InputMain = (int (*)(void *, const void *, size_t, int, int, int)) dlsym(mDolbyMS12LibHandle, "ms12_input_main");
    if (!FuncDolbyMS12InputMain) {
        ALOGE("%s, dlsym ms12_input_main fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12InputAssociate = (int (*)(void *, const void *, size_t, int, int, int)) dlsym(mDolbyMS12LibHandle, "ms12_input_associate");
    if (!FuncDolbyMS12InputAssociate) {
        ALOGE("%s, dlsym ms12_input_associate fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12InputSystem = (int (*)(void *, const void *, size_t, int, int, int)) dlsym(mDolbyMS12LibHandle, "ms12_input_system");
    if (!FuncDolbyMS12InputSystem) {
        ALOGE("%s, dlsym ms12_input_system fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12InputApp = (int (*)(void *, const void *, size_t, int, int, int)) dlsym(mDolbyMS12LibHandle, "ms12_input_app");
    if (!FuncDolbyMS12InputApp) {
        ALOGE("%s, dlsym ms12_input_app fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12FlushAppInputBuffer = (void (*)(void))  dlsym(mDolbyMS12LibHandle, "ms12_flush_app_input_buffer");
    if (!FuncDolbyMS12FlushAppInputBuffer) {
        ALOGE("%s, dlsym FuncDolbyMS12FlushAppInputBuffer fail\n", __FUNCTION__);
        goto ERROR;
    }

#ifdef REPLACE_OUTPUT_BUFFER_WITH_CALLBACK
    FuncDolbyMS12RegisterOutputCallback = (int (*)(output_callback , void *)) dlsym(mDolbyMS12LibHandle, "ms12_register_output_callback");
    if (!FuncDolbyMS12RegisterOutputCallback) {
        ALOGE("%s, dlsym ms12_output_register_output_callback fail\n", __FUNCTION__);
        goto ERROR;
    }
#else
    FuncDolbyMS12Output = (int (*)(void *, const void *, size_t))  dlsym(mDolbyMS12LibHandle, "ms12_output");
    if (!FuncDolbyMS12Output) {
        ALOGE("%s, dlsym ms12_output fail\n", __FUNCTION__);
        goto ERROR;
    }
#endif

    FuncDolbyMS12RegisterSyncCallback = (int (*)(void *, ms12sync_callback , void *)) dlsym(mDolbyMS12LibHandle, "ms12_register_sync_callback");
    if (!FuncDolbyMS12RegisterSyncCallback) {
        ALOGE("%s, dlsym ms12_output_register_sync_callback fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12UpdateRuntimeParams = (int (*)(void *, int , char **))  dlsym(mDolbyMS12LibHandle, "ms12_update_runtime_params");
    if (!FuncDolbyMS12UpdateRuntimeParams) {
        ALOGE("%s, dlsym ms12_update_runtime_params fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12UpdateRuntimeParamsNoLock = (int (*)(void *, int , char **))  dlsym(mDolbyMS12LibHandle, "ms12_update_runtime_params_nolock");
    if (!FuncDolbyMS12UpdateRuntimeParamsNoLock) {
        ALOGE("%s, dlsym ms12_update_runtime_params_nolock fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12SchedulerRun = (int (*)(void *))  dlsym(mDolbyMS12LibHandle, "ms12_scheduler_run");
    if (!FuncDolbyMS12SchedulerRun) {
        ALOGE("%s, dlsym ms12_scheduler_run fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12SetQuitFlag = (void (*)(int))  dlsym(mDolbyMS12LibHandle, "ms12_set_quit_flag");
    if (!FuncDolbyMS12SetQuitFlag) {
        ALOGE("%s, dlsym ms12_set_quit_flag fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12FlushInputBuffer = (void (*)(void))  dlsym(mDolbyMS12LibHandle, "ms12_flush_input_buffer");
    if (!FuncDolbyMS12FlushInputBuffer) {
        ALOGE("%s, dlsym ms12_flush_input_buffer fail\n", __FUNCTION__);
        goto ERROR;
    }
    FuncDolbyMS12FlushMainInputBuffer = (void (*)(void))  dlsym(mDolbyMS12LibHandle, "ms12_flush_main_input_buffer");
    if (!FuncDolbyMS12FlushInputBuffer) {
        ALOGE("%s, dlsym ms12_flush_main_input_buffer fail\n", __FUNCTION__);
        goto ERROR;
    }
    FuncDolbyMS12FlushAppInputBuffer = (void (*)(void))  dlsym(mDolbyMS12LibHandle, "ms12_flush_app_input_buffer");
    if (!FuncDolbyMS12FlushAppInputBuffer) {
        ALOGE("%s, dlsym FuncDolbyMS12FlushAppInputBuffer fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12GetNBytesConsumed = (unsigned long long (*)(void *, int, int))  dlsym(mDolbyMS12LibHandle, "get_decoder_n_bytes_consumed");
    if (!FuncDolbyMS12GetNBytesConsumed) {
        ALOGE("%s, dlsym get_decoder_n_bytes_consumed fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12GetPCMOutputSize = (void (*)(unsigned long long *, unsigned long long *))  dlsym(mDolbyMS12LibHandle, "get_pcm_output_size");
    if (!FuncDolbyMS12GetPCMOutputSize) {
        ALOGE("%s, dlsym get_pcm_output_size fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12GetBitstreamOutputSize = (void (*)(unsigned long long *, unsigned long long *))  dlsym(mDolbyMS12LibHandle, "get_bitstream_output_size");
    if (!FuncDolbyMS12GetBitstreamOutputSize) {
        ALOGE("%s, dlsym get_bitstream_output_size fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12GetMainBufferAvail = (int (*)(int *))  dlsym(mDolbyMS12LibHandle, "get_main_buffer_avail");
    if (!FuncDolbyMS12GetMainBufferAvail) {
        ALOGE("%s, dlsym get_main_buffer_avail fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12GetAssociateBufferAvail = (int (*)(void))  dlsym(mDolbyMS12LibHandle, "get_associate_buffer_avail");
    if (!FuncDolbyMS12GetAssociateBufferAvail) {
        ALOGE("%s, dlsym get_associate_buffer_avail fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12GetSystemBufferAvail = (int (*)(int *))  dlsym(mDolbyMS12LibHandle, "get_system_buffer_avail");
    if (!FuncDolbyMS12GetSystemBufferAvail) {
        ALOGE("%s, dlsym get_system_buffer_avail fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12GetGain = (int (*)(int))  dlsym(mDolbyMS12LibHandle, "ms12_get_gain_int");
    if (!FuncDolbyMS12GetGain) {
        ALOGE("%s, dlsym get_system_buffer_avail fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12Config = (int (*)(ms12_config_type_t, ms12_config_t *))  dlsym(mDolbyMS12LibHandle, "ms12_audio_config");
    if (!FuncDolbyMS12Config) {
        ALOGE("%s, dlsym ms12_audio_config\n", __FUNCTION__);
    }

    FuncDumpDolbyMS12Info = (int (*)(int))  dlsym(mDolbyMS12LibHandle, "dump_dolby_ms12_info");
    if (!FuncDumpDolbyMS12Info) {
        ALOGE("%s, dlsym dump_dolby_ms12_info\n", __FUNCTION__);
    }

    FuncDolbyMS12GetAudioInfo = (int (*)(struct aml_audio_info *))  dlsym(mDolbyMS12LibHandle, "get_audio_info");
    if (!FuncDolbyMS12GetAudioInfo) {
        ALOGE("%s, dlsym get_audio_info fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMS12GetMATDecLatency = (int (*)(void))  dlsym(mDolbyMS12LibHandle, "get_mat_dec_delay");
    if (!FuncDolbyMS12GetMATDecLatency) {
        ALOGE("%s, dlsym get_mat_dec_delay fail\n", __FUNCTION__);
    }


    FuncDolbyMS12GetNFramesPCMOutput = (unsigned long long (*)(void *, int, int))  dlsym(mDolbyMS12LibHandle, "get_decoder_n_frames_pcm_output");
    if (!FuncDolbyMS12GetNFramesPCMOutput) {
        ALOGE("%s, dlsym get_decoder_nframes_pcm_output fail\n", __FUNCTION__);
    }

    FuncDolbyMS12GetContinuousNFramesPCMOutput = (unsigned long long (*)(void *, int))  dlsym(mDolbyMS12LibHandle, "get_continuous_n_frames_pcm_output");
    if (!FuncDolbyMS12GetContinuousNFramesPCMOutput) {
        ALOGE("%s, dlsym get_continuous_n_frames_pcm_output fail\n", __FUNCTION__);
    }
    FuncDolbyMS12SetDebugLevel = (void (*)(int))  dlsym(mDolbyMS12LibHandle, "set_dolbyms12_debug_level");
    if (!FuncDolbyMS12SetDebugLevel) {
        ALOGE("%s, dlsym get_system_buffer_avail fail\n", __FUNCTION__);
    }

    FuncDolbyMS12GetNBytesConsumedSysSound = (unsigned long long (*)(void))  dlsym(mDolbyMS12LibHandle, "get_n_bytes_consumed_of_sys_sound");
    if (!FuncDolbyMS12GetNBytesConsumedSysSound) {
        ALOGW("%s, dlsym FuncDolbyMS12GetNBytesConsumedSysSound fail,ignore it as version difference\n", __FUNCTION__);
    }

    FuncDolbyMS12GetTotalNFramesDelay  = (int (*)(void *))  dlsym(mDolbyMS12LibHandle, "get_ms12_total_nframes_delay");
    if (!FuncDolbyMS12GetTotalNFramesDelay) {
        ALOGW("%s, dlsym get_ms12_total_delay fail, ignore it as version difference\n", __FUNCTION__);
    }

    FuncDolbyMS12HWSyncInit = (int (*)(void)) dlsym(mDolbyMS12LibHandle, "ms12_hwsync_init");
    if (!FuncDolbyMS12HWSyncInit) {
        ALOGW("%s, dlsym FuncDolbyMS12HWSyncInit fail,ignore it as version difference\n", __FUNCTION__);
    }

    FuncDolbyMS12HWSyncRelease = (int (*)(void)) dlsym(mDolbyMS12LibHandle, "ms12_hwsync_release");
    if (!FuncDolbyMS12HWSyncRelease) {
        ALOGW("%s, dlsym FuncDolbyMS12HWSyncRelease fail,ignore it as version difference\n", __FUNCTION__);
    }

    FuncDolbyMS12HWSyncCheckinPTS = (int (*)(int,  int))  dlsym(mDolbyMS12LibHandle, "ms12_hwsync_checkin_pts");
    if (!FuncDolbyMS12HWSyncCheckinPTS) {
        ALOGW("%s, dlsym FuncDolbyMS12HWSyncCheckinPTS fail,ignore it as version difference\n", __FUNCTION__);
    }

    FuncDolbyMS12SetPtsGap= (int (*)(unsigned long long, int)) dlsym(mDolbyMS12LibHandle, "ms12_set_pts_gap");
    if (!FuncDolbyMS12SetPtsGap) {
        ALOGW("%s, dlsym FuncDolbyMS12SetPtsGap fail\n", __FUNCTION__);
    }

    /* MAT Encoder API Begin */
    FuncDolbyMS12MATEncoderInit = (int (*)(int, int, unsigned int *, int, int, void **))  dlsym(mDolbyMS12LibHandle, "mat_encoder_init");
    if (!FuncDolbyMS12MATEncoderInit) {
        ALOGW("%s, dlsym FuncDolbyMS12MATEncoderInit fail,ignore it as version difference\n", __FUNCTION__);
    }

    FuncDolbyMS12MATEncoderCleanup = (int (*)(void *))  dlsym(mDolbyMS12LibHandle, "mat_encoder_cleanup");
    if (!FuncDolbyMS12MATEncoderCleanup) {
        ALOGW("%s, dlsym FuncDolbyMS12MATEncoderCleanup fail,ignore it as version difference\n", __FUNCTION__);
    }

    FuncDolbyMS12MATEncoderProcess = (int (*)(void *, const unsigned char *, int, const unsigned char *, int *, int, int *))  dlsym(mDolbyMS12LibHandle, "mat_encoder_process");
    if (!FuncDolbyMS12MATEncoderProcess) {
        ALOGW("%s, dlsym FuncDolbyMS12MATEncoderProcess fail,ignore it as version difference\n", __FUNCTION__);
    }

    FuncDolbyMS12MATEncoderConfig = (int (*)(void *, mat_enc_config_type_t, mat_enc_config_t *))  dlsym(mDolbyMS12LibHandle, "mat_encoder_config");
    if (!FuncDolbyMS12MATEncoderConfig) {
        ALOGW("%s, dlsym FuncDolbyMS12MATEncoderConfig fail,ignore it as version difference\n", __FUNCTION__);
    }
    /* MAT Encoder API End */

    FuncDolbyMs12EncoderOpen = (int (*)(void *, int argc, char **pp_argv)) dlsym(mDolbyMS12LibHandle, "ms12_encoder_open");
    if (!FuncDolbyMs12EncoderOpen) {
        ALOGE("%s, dlsym ms12_encoder_open fail\n", __FUNCTION__);
        goto ERROR;
    }

    FuncDolbyMs12EncoderClose = (int (*)(void *)) dlsym(mDolbyMS12LibHandle, "ms12_encoder_close");
    if (!FuncDolbyMs12EncoderClose) {
        ALOGE("%s, dlsym ms12_encoder_close fail\n", __FUNCTION__);
        goto ERROR;
    }
    FunDolbMS12GetVersion = (char * (*)(void)) dlsym(mDolbyMS12LibHandle, "ms12_get_version");
    if (!FunDolbMS12GetVersion) {
        ALOGW("%s, dlsym FunDolbMS12GetVersion fail, ignore it as version difference\n", __FUNCTION__);
    }

    FuncDolbyMS12RegisterCallbackbyType = (int (*)(int, void*, void *)) dlsym(mDolbyMS12LibHandle, "ms12_register_callback_by_type");
    if (!FuncDolbyMS12RegisterCallbackbyType) {
        ALOGW("[%s:%d], dlsym FuncDolbyMS12RegisterCallbackbyType failed", __FUNCTION__, __LINE__);
    }

    ALOGD("-%s() line %d get libdolbyms12 success!", __FUNCTION__, __LINE__);
    return 0;

ERROR:
    ALOGD("-%s() line %d", __FUNCTION__, __LINE__);
    return -1;
}

void DolbyMS12::ReleaseLibHandle(void)
{
    ALOGD("+%s()", __FUNCTION__);

    //re-value the api as NULL
    FuncGetMS12OutputMaxSize = NULL;
    FuncDolbyMS12Init = NULL;
    FuncDolbyMS12Release = NULL;
    FuncDolbyMS12InitAllParams = NULL;
    FuncDolbyMs12DecoderOpen = NULL;
    FuncDolbyMs12DecoderClose = NULL;
    FuncDolbyMs12DecoderProcess = NULL;
    FuncDolbyMS12InputMain = NULL;
    FuncDolbyMS12InputAssociate = NULL;
    FuncDolbyMS12InputSystem = NULL;
#ifdef REPLACE_OUTPUT_BUFFER_WITH_CALLBACK
    FuncDolbyMS12RegisterOutputCallback = NULL;
#else
    FuncDolbyMS12Output = NULL;
#endif
    FuncDolbyMS12RegisterSyncCallback = NULL;
    FuncDolbyMS12UpdateRuntimeParams = NULL;
    FuncDolbyMS12SchedulerRun = NULL;
    FuncDolbyMS12SetQuitFlag = NULL;
    FuncDolbyMS12FlushInputBuffer = NULL;
    FuncDolbyMS12GetNBytesConsumed = NULL;
    FuncDolbyMS12GetPCMOutputSize = NULL;
    FuncDolbyMS12GetBitstreamOutputSize = NULL;
    FuncDolbyMS12GetMainBufferAvail = NULL;
    FuncDolbyMS12GetAssociateBufferAvail = NULL;
    FuncDolbyMS12GetSystemBufferAvail = NULL;
    FuncDolbyMS12Config = NULL;
    FuncDumpDolbyMS12Info = NULL;
    FuncDolbyMS12GetAudioInfo = NULL;
    FuncDolbyMS12GetMATDecLatency = NULL;
    FunDolbMS12GetVersion = NULL;
    FuncDolbyMS12GetNFramesPCMOutput = NULL;
    FuncDolbyMS12SetDebugLevel = NULL;
    FuncDolbyMS12GetNBytesConsumedSysSound = NULL;
    FuncDolbyMS12GetTotalNFramesDelay = NULL;
    FuncDolbyMS12SetPtsGap = NULL;

    /* MAT Encoder API Begin */
    FuncDolbyMS12MATEncoderInit = NULL;
    FuncDolbyMS12MATEncoderCleanup = NULL;
    FuncDolbyMS12MATEncoderProcess = NULL;
    FuncDolbyMS12MATEncoderConfig = NULL;
    /* MAT Encoder API End */

    FuncDolbyMs12EncoderOpen = NULL;
    FuncDolbyMs12EncoderClose = NULL;
    FuncDolbyMS12GetContinuousNFramesPCMOutput = NULL;
    if (mDolbyMS12LibHandle != NULL) {
        dlclose(mDolbyMS12LibHandle);
        mDolbyMS12LibHandle = NULL;
    }

    ALOGD("-%s()", __FUNCTION__);
    return ;
}

int DolbyMS12::GetMS12OutputMaxSize(void)
{
    ALOGD("+%s()", __FUNCTION__);
    int ret = 0;
    if (!FuncGetMS12OutputMaxSize) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncGetMS12OutputMaxSize)();
    ALOGD("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}


void * DolbyMS12::DolbyMS12Init(int configNum, char **configParams)
{
    void * dolby_ms12_init_ret = NULL;
    ALOGD("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Init) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return NULL;
    }

    dolby_ms12_init_ret = (*FuncDolbyMS12Init)(configNum, configParams);
    ALOGD("-%s() dolby_ms12_init_ret %p", __FUNCTION__, dolby_ms12_init_ret);
    return dolby_ms12_init_ret;
}

char * DolbyMS12:: DolbMS12GetVersion(void)
{
    char *versioninfo = NULL;
    ALOGV("+%s()", __FUNCTION__);
    if (!FunDolbMS12GetVersion) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return NULL;
    }
    versioninfo = (*FunDolbMS12GetVersion)();
    return versioninfo;
}
void DolbyMS12::DolbyMS12Release(void *DolbyMS12Pointer)
{
    ALOGD("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Release) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ;
    }

    (*FuncDolbyMS12Release)(DolbyMS12Pointer);
    ALOGD("-%s()", __FUNCTION__);
    return ;
}


int DolbyMS12::DolbyMS12InitAllParams(void *DolbyMS12Pointer, int configNum, char **configParams)
{
    int ret = 0;
    ALOGD("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12InitAllParams) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMS12InitAllParams)(DolbyMS12Pointer, configNum, configParams);
    ALOGD("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMs12DecoderOpen(void *DolbyMS12Pointer, int configNum, char **configParams)
{
    ALOGV("+%s()", __FUNCTION__);
    int ret = 0;

    if (!FuncDolbyMs12DecoderOpen) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMs12DecoderOpen)(DolbyMS12Pointer, configNum, configParams);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMs12DecoderClose(void *DolbyMS12Pointer)
{
    ALOGV("+%s()", __FUNCTION__);
    int ret = 0;

    if (!FuncDolbyMs12DecoderClose) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMs12DecoderClose)(DolbyMS12Pointer);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMs12DecoderProcess(void *DolbyMS12Pointer)
{
    ALOGV("+%s()", __FUNCTION__);
    int ret = 0;

    if (!FuncDolbyMs12DecoderProcess) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMs12DecoderProcess)(DolbyMS12Pointer);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMs12EncoderOpen(void *DolbyMS12Pointer, int configNum, char **configParams)
{
    ALOGV("+%s()", __FUNCTION__);
    int ret = 0;

    if (!FuncDolbyMs12EncoderOpen) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMs12EncoderOpen)(DolbyMS12Pointer, configNum, configParams);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMs12EncoderClose(void *DolbyMS12Pointer)
{
    ALOGV("+%s()", __FUNCTION__);
    int ret = 0;

    if (!FuncDolbyMs12EncoderClose) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMs12EncoderClose)(DolbyMS12Pointer);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}


int DolbyMS12::DolbyMS12InputMain(
    void *DolbyMS12Pointer
    , const void *audio_stream_out_buffer //ms12 input buffer
    , size_t audio_stream_out_buffer_size //ms12 input buffer size
    , int audio_stream_out_format
    , int audio_stream_out_channel_num
    , int audio_stream_out_sample_rate
)
{
    ALOGV("+%s()", __FUNCTION__);
    int ret = 0;

    if (!FuncDolbyMS12InputMain) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMS12InputMain)(DolbyMS12Pointer
                                    , audio_stream_out_buffer //ms12 input buffer
                                    , audio_stream_out_buffer_size //ms12 input buffer size
                                    , audio_stream_out_format
                                    , audio_stream_out_channel_num
                                    , audio_stream_out_sample_rate);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12InputAssociate(
    void *DolbyMS12Pointer
    , const void *audio_stream_out_buffer //ms12 input buffer
    , size_t audio_stream_out_buffer_size //ms12 input buffer size
    , int audio_stream_out_format
    , int audio_stream_out_channel_num
    , int audio_stream_out_sample_rate
)
{
    ALOGV("+%s()", __FUNCTION__);
    int ret = 0;

    if (!FuncDolbyMS12InputAssociate) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMS12InputAssociate)(DolbyMS12Pointer
                                         , audio_stream_out_buffer //ms12 input buffer
                                         , audio_stream_out_buffer_size //ms12 input buffer size
                                         , audio_stream_out_format
                                         , audio_stream_out_channel_num
                                         , audio_stream_out_sample_rate);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12InputSystem(
    void *DolbyMS12Pointer
    , const void *audio_stream_out_buffer //ms12 input buffer
    , size_t audio_stream_out_buffer_size //ms12 input buffer size
    , int audio_stream_out_format
    , int audio_stream_out_channel_num
    , int audio_stream_out_sample_rate
)
{
    ALOGV("+%s()", __FUNCTION__);
    int ret = 0;

    if (!FuncDolbyMS12InputSystem) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMS12InputSystem)(DolbyMS12Pointer
                                      , audio_stream_out_buffer //ms12 input buffer
                                      , audio_stream_out_buffer_size //ms12 input buffer size
                                      , audio_stream_out_format
                                      , audio_stream_out_channel_num
                                      , audio_stream_out_sample_rate);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12InputApp(
    void *DolbyMS12Pointer
    , const void *audio_stream_out_buffer //ms12 input buffer
    , size_t audio_stream_out_buffer_size //ms12 input buffer size
    , int audio_stream_out_format
    , int audio_stream_out_channel_num
    , int audio_stream_out_sample_rate
)
{
    ALOGV("+%s()", __FUNCTION__);
    int ret = 0;

    if (!FuncDolbyMS12InputApp) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMS12InputApp)(DolbyMS12Pointer
                                      , audio_stream_out_buffer //ms12 input buffer
                                      , audio_stream_out_buffer_size //ms12 input buffer size
                                      , audio_stream_out_format
                                      , audio_stream_out_channel_num
                                      , audio_stream_out_sample_rate);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}


#ifdef REPLACE_OUTPUT_BUFFER_WITH_CALLBACK
int DolbyMS12::DolbyMS12RegisterOutputCallback(output_callback callback, void *priv_data)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12RegisterOutputCallback) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMS12RegisterOutputCallback)(callback, priv_data);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}
#else
int DolbyMS12::DolbyMS12Output(
    void *DolbyMS12Pointer
    , const void *ms12_out_buffer //ms12 output buffer
    , size_t request_out_buffer_size //ms12 output buffer size
)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Output) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMS12Output)(DolbyMS12Pointer
                                 , ms12_out_buffer //ms12 output buffer
                                 , request_out_buffer_size //ms12 output buffer size
                                );
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}
#endif

int DolbyMS12::DolbyMS12RegisterSyncCallback(void *DolbyMS12Pointer, ms12sync_callback callback, void *priv_data)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12RegisterSyncCallback) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMS12RegisterSyncCallback)(DolbyMS12Pointer, callback, priv_data);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12UpdateRuntimeParams(void *DolbyMS12Pointer, int configNum, char **configParams)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12UpdateRuntimeParams) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMS12UpdateRuntimeParams)(DolbyMS12Pointer, configNum, configParams);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12UpdateRuntimeParamsNoLock(void *DolbyMS12Pointer, int configNum, char **configParams)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12UpdateRuntimeParamsNoLock) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMS12UpdateRuntimeParamsNoLock)(DolbyMS12Pointer, configNum, configParams);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12SchedulerRun(void *DolbyMS12Pointer)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12SchedulerRun) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*FuncDolbyMS12SchedulerRun)(DolbyMS12Pointer);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

void DolbyMS12::DolbyMS12SetQuitFlag(int is_quit)
{
    int ret = 0;
    ALOGV("+%s() is_quit %d", __FUNCTION__, is_quit);
    if (!FuncDolbyMS12SetQuitFlag) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ;
    }

    (*FuncDolbyMS12SetQuitFlag)(is_quit);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ;
}

void DolbyMS12::DolbyMS12FlushInputBuffer(void)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12FlushInputBuffer) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ;
    }

    (*FuncDolbyMS12FlushInputBuffer)();
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ;
}
void DolbyMS12::DolbyMS12FlushMainInputBuffer(void)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12FlushMainInputBuffer) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ;
    }

    (*FuncDolbyMS12FlushMainInputBuffer)();
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ;
}

void DolbyMS12::DolbyMS12FlushAppInputBuffer(void)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12FlushAppInputBuffer) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ;
    }

    (*FuncDolbyMS12FlushAppInputBuffer)();
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ;
}

unsigned long long DolbyMS12::DolbyMS12GetDecoderNBytesConsumed(void *ms12_pointer, int format, int is_main)
{
    unsigned long long ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12GetNBytesConsumed) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12GetNBytesConsumed)(ms12_pointer, format, is_main);
    ALOGV("-%s() ret %llu", __FUNCTION__, ret);
    return ret;
}

/*
idx = 0, primary
idx = 1, secondary
idx = 2, system
*/
int DolbyMS12::DolbyMS12GetGain(int idx)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12GetGain) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12GetGain)(idx);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

void DolbyMS12::DolbyMS12GetPCMOutputSize(unsigned long long *all_output_size, unsigned long long *ms12_generate_zero_size)
{
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12GetPCMOutputSize) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ;
    }

    (*FuncDolbyMS12GetPCMOutputSize)(all_output_size, ms12_generate_zero_size);
    ALOGV("-%s() *all_output_size %llu *ms12_generate_zero_size %llu", __FUNCTION__, *all_output_size,  *ms12_generate_zero_size);
    return ;
}


void DolbyMS12::DolbyMS12GetBitstreamOutputSize(unsigned long long *all_output_size, unsigned long long *ms12_generate_zero_size)
{
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12GetBitstreamOutputSize) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ;
    }

    (*FuncDolbyMS12GetBitstreamOutputSize)(all_output_size, ms12_generate_zero_size);
    ALOGV("-%s() *all_output_size %llu *ms12_generate_zero_size %llu", __FUNCTION__, *all_output_size,  *ms12_generate_zero_size);
    return ;
}

int DolbyMS12::DolbyMS12GetMainBufferAvail(int * max_size)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12GetMainBufferAvail) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12GetMainBufferAvail)(max_size);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12GetAssociateBufferAvail(void)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12GetAssociateBufferAvail) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12GetAssociateBufferAvail)();
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12GetSystemBufferAvail(int * max_size)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12GetSystemBufferAvail) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12GetSystemBufferAvail)(max_size);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12SetMainVolume(float volume)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_MAIN_VOLUME, (ms12_config_t *)&volume);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12SetMATStreamProfile(int stream_profile)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_MAT_STREAM_PROFILE, (ms12_config_t *)&stream_profile);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12SetAtmosDrop(int atmos_drop)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_ATMOS_DROP, (ms12_config_t *)&atmos_drop);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

void DolbyMS12::DumpDolbyMS12Info(int fd)
{
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDumpDolbyMS12Info) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
    }
    else {
        (*FuncDumpDolbyMS12Info)(fd);
    }

}

int DolbyMS12::DolbyMS12GetInputISDolbyAtmos()
{
    int ret = 0;
    struct aml_audio_info p_aml_audio_info;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12GetAudioInfo) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12GetAudioInfo)(&p_aml_audio_info);
    ALOGV("-%s() ret %d atmos Detected %d", __FUNCTION__, ret, p_aml_audio_info.is_dolby_atmos);
    return p_aml_audio_info.is_dolby_atmos;
}

int DolbyMS12::DolbyMS12GetMATDecLatency()
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12GetMATDecLatency) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12GetMATDecLatency)();
    return ret;
}


int DolbyMS12::DolbyMS12EnableMixerMaxSize(int enable)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_MIXER_MAX_SIZE_ENABLED, (ms12_config_t *)&enable);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12SetCompressionFormat(int compression_format)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_COMPRESSION_FORMAT, (ms12_config_t *)&compression_format);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12SetSchedulerState(int sch_state)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_SCHEDULER_STATE, (ms12_config_t *)&sch_state);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

unsigned long long DolbyMS12::DolbyMS12GetDecoderNFramesPcmOutput(void *ms12_pointer, int format, int is_main)
{
    uint64_t ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12GetNFramesPCMOutput) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12GetNFramesPCMOutput)(ms12_pointer, format, is_main);
    ALOGV("-%s() ret %" PRId64 "", __FUNCTION__, ret);
    return ret;
}

unsigned long long DolbyMS12::DolbyMS12GetContinuousNFramesPcmOutput(void *ms12_pointer, int index)
{
    uint64_t ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12GetContinuousNFramesPCMOutput) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12GetContinuousNFramesPCMOutput)(ms12_pointer, index);
    ALOGV("-%s() ret %" PRId64 "", __FUNCTION__, ret);
    return ret;
}


void DolbyMS12::DolbyMS12SetDebugLevel(int level)
{
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12SetDebugLevel) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
    }
    else
        (*FuncDolbyMS12SetDebugLevel)(level);
}

unsigned long long DolbyMS12::DolbyMS12GetNBytesConsumedSysSound(void)
{
    unsigned long long ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12GetNBytesConsumedSysSound) {
        return ret;
    }

    ret = (*FuncDolbyMS12GetNBytesConsumedSysSound)();
    ALOGV("-%s() ret %llu", __FUNCTION__, ret);
    return ret;
}


int DolbyMS12::DolbyMS12GetTotalNFramesDelay(void *ms12_pointer)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12GetTotalNFramesDelay) {
        return -1;
    }

    ret = (*FuncDolbyMS12GetTotalNFramesDelay)(ms12_pointer);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12HWSyncInit(void)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12HWSyncInit) {
        //ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12HWSyncInit)();
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12HWSyncRelease(void)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12HWSyncRelease) {
        //ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12HWSyncRelease)();
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12HWSyncCheckinPTS(int offset, int apts)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12HWSyncRelease) {
        //ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12HWSyncCheckinPTS)(offset, apts);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12GetLatencyForStereoOut(int *latency)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_STEREO_OUT_LATENCY, (ms12_config_t *)latency);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}


int DolbyMS12::DolbyMS12GetLatencyForMultiChannelOut(int *latency)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_MULTICHANNEL_OUT_LATENCY, (ms12_config_t *)latency);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12SetPtsGap(unsigned long long offset, int pts_duration)
{
    int ret = -1;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12SetPtsGap) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12SetPtsGap)(offset, pts_duration);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12GetMainAudioInfo(int *sample_rate, int *acmod, int *b_lfe)
{
    int ret = 0;
    struct aml_audio_info p_aml_audio_info;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12GetAudioInfo) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12GetAudioInfo)(&p_aml_audio_info);
    *sample_rate = p_aml_audio_info.sample_rate;
    *acmod       = p_aml_audio_info.acmod;
    *b_lfe       = p_aml_audio_info.b_lfe;
    ret = (*FuncDolbyMS12GetAudioInfo)(&p_aml_audio_info);
    ALOGV("-%s() %d %d %d", __FUNCTION__, *sample_rate, *acmod, *b_lfe);
    return ret;
}
int DolbyMS12::DolbyMS12GetLatencyForDAPSpeakerOut(int *latency)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_DAP_SPEAKER_OUT_LATENCY, (ms12_config_t *)latency);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12GetLatencyForDAPHeadphoneOut(int *latency)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_DAP_HEADPHONE_OUT_LATENCY, (ms12_config_t *)latency);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12GetLatencyForDDPOut(int *latency)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_DDP_OUT_LATENCY, (ms12_config_t *)latency);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12GetLatencyForDDOut(int *latency)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_DD_OUT_LATENCY, (ms12_config_t *)latency);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12GetLatencyForMATOut(int *latency)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_MAT_OUT_LATENCY, (ms12_config_t *)latency);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}


/* MAT Encoder API Begin */
int DolbyMS12::DolbyMS12MATEncoderInit(int b_lfract_precision
    , int b_chmod_locking
    , unsigned int *p_matenc_maxoutbufsize
    , int b_iec_header
    , int dbg_enable
    , void **mat_enc_handle)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12MATEncoderInit) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12MATEncoderInit)(b_lfract_precision, b_chmod_locking, p_matenc_maxoutbufsize, b_iec_header, dbg_enable, mat_enc_handle);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

void DolbyMS12::DolbyMS12MatEncoderCleanup(void *mat_enc_handle)
{
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12MATEncoderCleanup) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
    }
    else {
        (*FuncDolbyMS12MATEncoderCleanup)(mat_enc_handle);
    }
}

int DolbyMS12::DolbyMS12MATEncoderProcess(void *mat_enc_handle
    , const unsigned char *in_buf
    , int n_bytes_in_buf
    , const unsigned char *out_buf
    , int *n_bytes_out_buf
    , int out_buf_max_size
    , int *nbytes_consumed
    )
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12MATEncoderProcess) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12MATEncoderProcess)(mat_enc_handle, in_buf, n_bytes_in_buf, out_buf, n_bytes_out_buf, out_buf_max_size, nbytes_consumed);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12MATEncoderConfig(void *mat_enc_handle
    , mat_enc_config_type_t config_type
    , mat_enc_config_t *config)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12MATEncoderConfig) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12MATEncoderConfig)(mat_enc_handle, config_type, config);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

/* MAT Encoder API End */

int DolbyMS12::DolbyMS12GetAC4ActivePresentation(int *presentation_group_index)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_AC4DEC_GET_ACTIVE_PRESENTATION, (ms12_config_t *)presentation_group_index);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12AC4DecCheckThePgiIsPresent(int presentation_group_index)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_AC4DEC_CHECK_THE_PGI_IS_PRESENT, (ms12_config_t *)&presentation_group_index);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12SetAlsaDelayFrame(int delay_frame)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_ALSA_DELAY_FRAME, (ms12_config_t *)&delay_frame);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12RegisterCallbackbytype(int type, void* callback, void *priv_data)
{
    int ret = 0;
    ALOGI("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12RegisterCallbackbyType) {
        return ret;
    }

    ret = (*FuncDolbyMS12RegisterCallbackbyType)(type, callback, priv_data);
    ALOGI("ret %d, type:%d, callback:%p", ret, type, callback);
    return ret;
}

int DolbyMS12::DolbyMS12SetAlsaLimitFrame(int limit_frame)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_ALSA_LIMIT_FRAME, (ms12_config_t *)&limit_frame);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

int DolbyMS12::DolbyMS12SetSchedulerSleep(int enable_sleep)
{
    int ret = 0;
    ALOGV("+%s()", __FUNCTION__);
    if (!FuncDolbyMS12Config) {
        ALOGE("%s(), pls load lib first.\n", __FUNCTION__);
        return ret;
    }

    ret = (*FuncDolbyMS12Config)(MS12_CONFIG_SCHEDULER_SLEEP, (ms12_config_t *)&enable_sleep);
    ALOGV("-%s() ret %d", __FUNCTION__, ret);
    return ret;
}

/*--------------------------------------------------------------------------*/
}   // namespace android
