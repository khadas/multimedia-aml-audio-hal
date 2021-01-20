/*
 * Copyright (C) 2010 Amlogic Corporation.
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



#ifndef  _AUDIO_HW_UTILS_H_
#define _AUDIO_HW_UTILS_H_
#include <system/audio.h>
#include "audio_hw.h"
#include "aml_audio_types_def.h"
#include "aml_audio_stream.h"

#define ENUM_TYPE_STR_MAX_LEN                           (100)

#define ENUM_TYPE_TO_STR(x, offset, pStr)                   \
case x: {                                                   \
    pStr = #x;                                              \
    pStr += offset;                                         \
    if (strlen(#x) - offset > 70) {                         \
        pStr += 70;                                         \
    }                                                       \
    break;                                                  \
}

int64_t aml_gettime(void);
int get_sysfs_uint(const char *path, uint *value);
int sysfs_set_sysfs_str(const char *path, const char *val);
int get_sysfs_int(const char *path);
int mystrstr(char *mystr, char *substr) ;
void set_codec_type(int type);
int get_codec_type(int format);
int getprop_bool(const char *path);
unsigned char codec_type_is_raw_data(int type);
int mystrstr(char *mystr, char *substr);
void *convert_audio_sample_for_output(int input_frames, int input_format, int input_ch, void *input_buf, int *out_size/*,float lvol*/);
int  aml_audio_start_trigger(void *stream);
int is_txlx_chip();
int is_txl_chip();
int is_sc2_chip();
int aml_audio_get_debug_flag();
int aml_audio_debug_set_optical_format();
int aml_audio_dump_audio_bitstreams(const char *path, const void *buf, size_t bytes);
int aml_audio_get_ms12_latency_offset(int b_raw);
int aml_audio_get_ms12_tunnel_latency_offset(int b_raw);
int aml_audio_get_ms12_atmos_latency_offset(int tunnel);

int aml_audio_get_ddp_frame_size();
bool is_stream_using_mixer(struct aml_stream_out *out);
uint32_t out_get_outport_latency(const struct audio_stream_out *stream);
uint32_t out_get_latency_frames(const struct audio_stream_out *stream);
int aml_audio_get_spdif_tuning_latency(void);
int aml_audio_get_tvsrc_tune_latency(enum patch_src_assortion patch_src);
int sysfs_get_sysfs_str(const char *path, const char *val, int len);
void audio_fade_func(void *buf,int fade_size,int is_fadein);
void ts_wait_time_us(struct timespec *ts, uint32_t time_us);
int cpy_16bit_data_with_gain(int16_t *dst, int16_t *src, int size_in_bytes, float vol);
uint64_t get_systime_ns(void);
int aml_audio_get_hdmi_latency_offset(audio_format_t  fmt);
int aml_audio_get_speaker_latency_offset(audio_format_t fmt);
uint32_t tspec_diff_to_us(struct timespec tval_old,
        struct timespec tval_new);
void aml_audio_switch_output_mode(int16_t *buf, size_t bytes, AM_AOUT_OutputMode_t mode);
void * aml_audio_get_muteframe(audio_format_t output_format, int * frame_size, int bAtmos);
uint32_t out_get_ms12_latency_frames(const struct audio_stream_out *stream);
int aml_audio_get_ms12_latency_offset(int b_raw);
int aml_audio_get_dolby_drc_mode(int *drc_mode, int *drc_cut, int *drc_boost);

int aml_audio_get_video_latency(void);
int aml_set_thread_priority(char *pName, pthread_t threadId);

void aml_tinymix_set_spdif_format(audio_format_t output_format,  struct aml_stream_out *stream);
void aml_tinymix_set_spdifb_format(audio_format_t output_format,  struct aml_stream_out *stream);

void aml_audio_set_cpu23_affinity();


int aml_audio_get_ms12_passthrough_latency(struct audio_stream_out *stream);
int aml_audio_get_atmos_hdmi_latency_offset(audio_format_t fmt, int bypass);

struct pcm_config update_earc_out_config(struct pcm_config *config);
int continous_mode(struct aml_audio_device *adev);
bool direct_continous(struct audio_stream_out *stream);
bool primary_continous(struct audio_stream_out *stream);
int dolby_stream_active(struct aml_audio_device *adev);
int hwsync_lpcm_active(struct aml_audio_device *adev);
struct aml_stream_out *direct_active(struct aml_audio_device *adev);
#endif
