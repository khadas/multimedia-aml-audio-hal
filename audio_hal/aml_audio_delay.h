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
#include "alsa_manager.h"


typedef struct AML_AUDIO_DELAY {
    int                 delay_time;
    struct ring_buffer  stDelayRbuffer;
    int                 is_init_buffer;
} aml_audio_delay_st;


typedef enum AML_AUDIO_DELAY_TYPE{
    AML_DELAY_OUTPORT_SPEAKER           = 0, // for speaker output pcm case
    AML_DELAY_OUTPORT_SPDIF             = 1, // for spdif/hdmi output pcm case, note: spdif/spdif raw/spdif b raw use same delay value
    AML_DELAY_OUTPORT_HEADPHONE         = 2, // for headphone output pcm case
    AML_DELAY_OUTPORT_SPDIF_RAW         = 3, // for spdif/hdmi output DD case, note: spdif/spdif raw/spdif b raw use same delay value
    AML_DELAY_OUTPORT_SPDIF_B_RAW       = 4, // for hdmi/arc output DDP/MAT case, note: spdif/spdif raw/spdif b raw use same delay value

    AML_DELAY_OUTPORT_BUTT              = 5,
} aml_audio_delay_type_e;

int aml_audio_delay_init(int s32MaxDelayMs);
int aml_audio_delay_deinit();
int aml_audio_delay_set_time(aml_audio_delay_type_e enAudioDelayType, int s32DelayTimeMs);
int aml_audio_set_port_delay(char* port_name, int s32DelayTimeMs);
int aml_audio_delay_clear(aml_audio_delay_type_e enAudioDelayType);
int aml_audio_delay_process(aml_audio_delay_type_e enAudioDelayType, void *pData, int s32Size,
        audio_format_t enFormat, uint32_t sample_rate);

#endif

