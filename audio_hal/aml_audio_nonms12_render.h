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
#ifndef _AML_AUDIO_NONMS12_RENDER_H_
#define _AML_AUDIO_NONMS12_RENDER_H_

#include "audio_hw.h"

/**
 * @brief This function use aml audio decoder to process the audio data
 *
 * @returns the process result
 */
int aml_audio_nonms12_render(struct audio_stream_out *stream, struct audio_buffer *abuffer);
bool aml_decoder_output_compatible(struct audio_stream_out *stream, audio_format_t sink_format, audio_format_t optical_format);
int aml_decoder_config_prepare(struct audio_stream_out *stream, audio_format_t format, aml_dec_config_t * dec_config);
int dca_get_out_ch_internal(void);
int dca_set_out_ch_internal(int ch_num);
void aml_audio_nonms12_output(struct audio_stream_out *stream);
int aml_audio_nonms12_dec_render(struct audio_stream_out *stream, struct audio_buffer *abuffer);

#endif

