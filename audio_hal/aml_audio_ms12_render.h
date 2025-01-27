/*
 * Copyright (C) 2021 Amlogic Corporation.
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
#ifndef _AML_AUDIO_MS12_RENDER_H_
#define _AML_AUDIO_MS12_RENDER_H_

#include "audio_hw.h"

/**
 * @brief This function use aml audio decoder to process the audio data
 *
 * @returns the process result
 */
int aml_audio_ms12_render(struct audio_stream_out *stream, struct audio_buffer *abuffer);
int aml_audio_ad_render(struct audio_stream_out *stream, struct audio_buffer *abuffer);

#endif

