/*
 * Copyright (C) 2018 Amlogic Corporation.
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


#ifndef AML_AUDIO_DELAY_H
#define AML_AUDIO_DELAY_H
#include <system/audio-base.h>
#include <aml_ringbuffer.h>


typedef struct AML_AUDIO_DELAY {
    int                 delay_time;
    struct ring_buffer  stDelayRbuffer;
} aml_audio_delay_st;


typedef enum AML_AUDIO_DELAY_TYPE {
    AML_DELAY_OUTPORT_SPEAKER           = 0,
    AML_DELAY_OUTPORT_SPDIF             = 1,
    AML_DELAY_OUTPORT_ALL               = 2,
    AML_DELAY_INPORT_ALL                = 3,
    AML_DELAY_BUTT                      = 4,
} aml_audio_delay_type_e;

typedef enum AML_AUDIO_INPUT_DELAY_TYPE {
    AML_DELAY_INPUT_HDMI                = 0,
    AML_DELAY_INPUT_ARC                 = 1,
    AML_DELAY_INPUT_OPT                 = 2,
    AML_DELAY_INPUT_HDMI_BT             = 3,
    AML_DELAY_INPUT_DEFAULT             = 4,
    AML_DELAY_INPUT_MAX                 = 5
}  aml_audio_delay_input_type_e;

int aml_audio_delay_init();
int aml_audio_delay_deinit();
int aml_audio_delay_set_time(aml_audio_delay_type_e enAudioDelayType, int s32DelayTimeMs);
int aml_audio_delay_input_set_time(aml_audio_delay_input_type_e enAudioInputDelayType, int s32DelayTimeMs);
int aml_audio_delay_input_set_type(aml_audio_delay_input_type_e enAudioDelayType);
int aml_audio_delay_input_get_ms();
int aml_audio_delay_clear(aml_audio_delay_type_e enAudioDelayType);
int aml_audio_delay_process(aml_audio_delay_type_e enAudioDelayType, void *pData, int s32Size, audio_format_t enFormat, int nChannel);
int aml_audio_delay_input_process(void *pData, int s32Size, audio_format_t enFormat, int nChannel, int nsampleRate, int nSampleSize);

#endif

