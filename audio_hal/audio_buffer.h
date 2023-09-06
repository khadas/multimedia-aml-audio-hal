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

#ifndef _AUDIO_BUFFER_
#define _AUDIO_BUFFER_


struct audio_buffer {
    bool b_pts_valid;
    int format;
    ssize_t size;
    uint64_t pts; //unit 90k;
    const void* buffer;
};

#endif
