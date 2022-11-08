/*
 * Copyright (C) 2022 Amlogic Corporation.
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

#ifndef _AML_ESMODE_SYNC_H_
#define _AML_ESMODE_SYNC_H_
#include "audio_hwsync.h"
#include "audio_hw_ms12_v2.h"

sync_process_res aml_hwsynces_ms12_process_policy(void *priv_data, aml_ms12_dec_info_t *ms12_info, struct aml_stream_out *aml_out_write);
void aml_hwsynces_ms12_get_policy(struct audio_stream_out *stream);
sync_process_res  aml_hwmediasync_nonms12_process(struct audio_stream_out *stream, int duration, bool *speed_enabled);

#endif //end of _AML_ESMODE_SYNC_H_