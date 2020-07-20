/*
 * Copyright (C) 2018-2020 Amlogic Corporation.
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

#ifndef _AUDIO_HW_INSERT_SILENCE_DATA_H_
#define _AUDIO_HW_INSERT_SILENCE_DATA_H_


/*
 *@brief insert silence data
 *
 * input parameters
 *     stream: audio_stream_out handle
 *     buffer: data buffer address
 *     bytes: data size
 *     adjust_ms: duration of the insert data
 *     output_format: data format
 * output parameters
 *     retunr: -1 occure some error
 *              0 success
 */

int insert_silence_data(struct audio_stream_out *stream
                  , const void *buffer
                  , size_t bytes
                  , int  adjust_ms
                  , audio_format_t output_format);

#endif

