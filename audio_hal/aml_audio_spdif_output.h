/*
 * Copyright (C) 2020 Amlogic Corporation.
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
#ifndef _AML_AUDIO_SPDIF_OUTPUT_H_
#define _AML_AUDIO_SPDIF_OUTPUT_H_

void  aml_audio_spdif_output_stop(struct audio_stream_out *stream);
int   aml_audio_spdif_output_start(struct audio_stream_out *stream, audio_format_t output_format);
ssize_t aml_audio_spdif_output_direct(struct audio_stream_out *stream,
                                      void *buffer, size_t byte, audio_format_t output_format);
ssize_t aml_audio_spdif_output(struct audio_stream_out *stream,
                               void *buffer, size_t byte, audio_format_t output_format);


#endif
