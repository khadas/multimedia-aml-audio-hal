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


#define OUTPUT_DELAY_MAX_MS     (1000)
#define OUTPUT_DELAY_MIN_MS     (0)
#define ONE_FRAME_MAX_MS        (100)

typedef struct AML_AUDIO_DELAY {
    int delay_time;
    int delay_max;
    struct ring_buffer stDelayRbuffer;
} aml_audio_delay_st;


int aml_audio_delay_init(aml_audio_delay_st **ppstAudioOutputDelayHandle);
int aml_audio_delay_close(aml_audio_delay_st **ppstAudioOutputDelayHandle);
int aml_audio_delay_set_time(aml_audio_delay_st **ppstAudioOutputDelayHandle, int s32DelayTimeMs);
int aml_audio_delay_process(aml_audio_delay_st *pstAudioDelayHandle, void * in_data, int size, audio_format_t format);


#endif

