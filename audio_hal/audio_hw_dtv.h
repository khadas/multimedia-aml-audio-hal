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

#ifndef _AUDIO_HW_DTV_H_
#define _AUDIO_HW_DTV_H_

enum {
    AUDIO_DTV_PATCH_DECODER_STATE_INIT,
    AUDIO_DTV_PATCH_DECODER_STATE_START,
    AUDIO_DTV_PATCH_DECODER_STATE_RUNING,
    AUDIO_DTV_PATCH_DECODER_STATE_PAUSE,
    AUDIO_DTV_PATCH_DECODER_STATE_RESUME,
    AUDIO_DTV_PATCH_DECODER_STATE_RELEASE,
};

/* refer to AudioSystemCmdManager */
typedef enum {
    AUDIO_DTV_PATCH_CMD_NULL        = 0,
    AUDIO_DTV_PATCH_CMD_START       = 1,    /* AUDIO_SERVICE_CMD_START_DECODE */
    AUDIO_DTV_PATCH_CMD_PAUSE       = 2,    /* AUDIO_SERVICE_CMD_PAUSE_DECODE */
    AUDIO_DTV_PATCH_CMD_RESUME      = 3,    /* AUDIO_SERVICE_CMD_RESUME_DECODE */
    AUDIO_DTV_PATCH_CMD_STOP        = 4,    /* AUDIO_SERVICE_CMD_STOP_DECODE */
    AUDIO_DTV_PATCH_CMD_SET_AD_SUPPORT  = 5,    /* AUDIO_SERVICE_CMD_SET_DECODE_AD */
    AUDIO_DTV_PATCH_CMD_SET_VOLUME  = 6,    /*AUDIO_SERVICE_CMD_SET_VOLUME*/
    AUDIO_DTV_PATCH_CMD_SET_MUTE    = 7,    /*AUDIO_SERVICE_CMD_SET_MUTE*/
    AUDIO_DTV_PATCH_CMD_SET_OUTPUT_MODE = 8,/*AUDIO_SERVICE_CMD_SET_OUTPUT_MODE */
    AUDIO_DTV_PATCH_CMD_SET_PRE_GAIN  = 9,    /*AUDIO_SERVICE_CMD_SET_PRE_GAIN */
    AUDIO_DTV_PATCH_CMD_SET_PRE_MUTE  = 10,  /*AUDIO_SERVICE_CMD_SET_PRE_MUTE */
    AUDIO_DTV_PATCH_CMD_OPEN        = 12,   /*AUDIO_SERVICE_CMD_OPEN_DECODER */
    AUDIO_DTV_PATCH_CMD_CLOSE       = 13,   /*AUDIO_SERVICE_CMD_CLOSE_DECODER */
    AUDIO_DTV_PATCH_CMD_SET_DEMUX_INFO = 14, /*AUDIO_SERVICE_CMD_SET_DEMUX_INFO ;*/
    AUDIO_DTV_PATCH_CMD_SET_SECURITY_MEM_LEVEL = 15,/*AUDIO_SERVICE_CMD_SET_SECURITY_MEM_LEVEL*/
    AUDIO_DTV_PATCH_CMD_SET_HAS_VIDEO   = 16,/*AUDIO_SERVICE_CMD_SET_HAS_VIDEO */
    AUDIO_DTV_PATCH_CMD_CONTROL       = 17,
    AUDIO_DTV_PATCH_CMD_SET_PID       = 18,
    AUDIO_DTV_PATCH_CMD_SET_FMT        = 19,
    AUDIO_DTV_PATCH_CMD_SET_AD_PID      = 20,
    AUDIO_DTV_PATCH_CMD_SET_AD_FMT      = 21,
    AUDIO_DTV_PATCH_CMD_SET_AD_ENABLE      = 22,
    AUDIO_DTV_PATCH_CMD_SET_AD_MIX_LEVEL   = 23,
    AUDIO_DTV_PATCH_CMD_SET_AD_VOL_LEVEL   = 24,
    AUDIO_DTV_PATCH_CMD_SET_MEDIA_SYNC_ID   = 25,
    AUDIO_DTV_PATCH_CMD_SET_MEDIA_PRESENTATION_ID   = 26,
    AUDIO_DTV_PATCH_CMD_SET_DTV_LATENCYMS_ID = 27,
    AUDIO_DTV_PATCH_CMD_SET_MEDIA_FIRST_LANG  = 29,
    AUDIO_DTV_PATCH_CMD_SET_MEDIA_SECOND_LANG = 30,
    AUDIO_DTV_PATCH_CMD_SET_SPDIF_PROTECTION_MODE  = 31,
    AUDIO_DTV_PATCH_CMD_NUM             = 32,
} AUDIO_DTV_PATCH_CMD_TYPE;

enum {
    AVSYNC_ACTION_NORMAL,
    AVSYNC_ACTION_DROP,
    AVSYNC_ACTION_HOLD,
};
enum {
    DIRECT_SPEED = 0, // DERIECT_SPEED
    DIRECT_SLOW,
    DIRECT_NORMAL,
};
enum {
    AUDIO_FREE = 0,
    AUDIO_BREAK,
    AUDIO_LOOKUP,
    AUDIO_DROP,
    AUDIO_RAISE,
    AUDIO_LATENCY,
    AUDIO_RUNNING,
};
enum {
    TSYNC_MODE_VMASTER = 0,
    TSYNC_MODE_AMASTER,
    TSYNC_MODE_PCRMASTER,
};
typedef struct ps_alloc_para {
    uint32_t mMaxCount;
    uint32_t mLookupThreshold;
    uint32_t kDoubleCheckThreshold;
} ptsserver_alloc_para;
typedef struct checkoutptsoffset {
    uint64_t offset;
    uint64_t pts_90k;
    uint64_t pts_64;
} checkout_pts_offset;

#ifdef BUILD_LINUX
#ifndef __unused
#define __unused __attribute__((unused))
#endif
#endif
int create_dtv_patch(struct audio_hw_device *dev, audio_devices_t input, audio_devices_t output __unused);
int release_dtv_patch(struct aml_audio_device *dev);
int release_dtv_patch_l(struct aml_audio_device *dev);
//int dtv_patch_add_cmd(int cmd);
int dtv_in_read(struct audio_stream_in *stream, void* buffer, size_t bytes);
void dtv_in_write(struct audio_stream_out *stream, const void* buffer, size_t bytes);
int audio_set_spdif_clock(struct aml_stream_out *stream,int type);

extern size_t aml_alsa_output_write(struct audio_stream_out *stream, void *buffer, size_t bytes);

extern void dtv_adjust_output_clock(struct aml_audio_patch * patch, int direct, int step, bool is_dual);

int dtv_patch_handle_event(struct audio_hw_device *dev,int cmd, int val);
int dtv_patch_get_latency(struct aml_audio_device *aml_dev);
bool dtv_is_secure(void *dtv_instances);
int dtv_patch_get_es_pts_dts_flag(struct aml_audio_device *aml_dev);

#endif
