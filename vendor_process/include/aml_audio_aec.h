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

#ifndef AML_AUDIO_AEC_H
#define AML_AUDIO_AEC_H

#include <stdint.h>
#include <tinyalsa/asoundlib.h>

#define AML_AEC_LIB_PATH "/usr/lib/libAudioSignalProcess.so"

struct aec_context {
    void *aml_aec;
    int mic_channels; // number of mic channels for loopback
    struct pcm_config config;
};

struct aec_context* aec_create(int mic_channels, struct pcm_config config);
void aec_destroy(struct aec_context *aec);
int aec_process(struct aec_context* aec, void* in, void* out);

#endif /* AML_AUDIO_AEC_H */

