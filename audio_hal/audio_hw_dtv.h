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

//{reference from " /amcodec/include/amports/aformat.h"
#define ACODEC_FMT_NULL -1
#define ACODEC_FMT_MPEG 0
#define ACODEC_FMT_PCM_S16LE 1
#define ACODEC_FMT_AAC 2
#define ACODEC_FMT_AC3 3
#define ACODEC_FMT_ALAW 4
#define ACODEC_FMT_MULAW 5
#define ACODEC_FMT_DTS 6
#define ACODEC_FMT_PCM_S16BE 7
#define ACODEC_FMT_FLAC 8
#define ACODEC_FMT_COOK 9
#define ACODEC_FMT_PCM_U8 10
#define ACODEC_FMT_ADPCM 11
#define ACODEC_FMT_AMR 12
#define ACODEC_FMT_RAAC 13
#define ACODEC_FMT_WMA 14
#define ACODEC_FMT_WMAPRO 15
#define ACODEC_FMT_PCM_BLURAY 16
#define ACODEC_FMT_ALAC 17
#define ACODEC_FMT_VORBIS 18
#define ACODEC_FMT_AAC_LATM 19
#define ACODEC_FMT_APE 20
#define ACODEC_FMT_EAC3 21
#define ACODEC_FMT_WIFIDISPLAY 22
#define ACODEC_FMT_DRA 23
#define ACODEC_FMT_TRUEHD 25
#define ACODEC_FMT_MPEG1 26 // AFORMAT_MPEG-->mp3,AFORMAT_MPEG1-->mp1,AFROMAT_MPEG2-->mp2
#define ACODEC_FMT_MPEG2 27
#define ACODEC_FMT_WMAVOI 28
#define ACODEC_FMT_AC4    29

//}
#define DTV_AUDIO_JUMPED_DEFAULT_THRESHOLD   (400)  //ms
#define DTV_AUDIO_RETUNE_DEFAULT_THRESHOLD   (300)  //ms
#define DTV_AUDIO_DROP_HOLD_LEAST_MS         (32 * 4) //ms
#define DTV_AV_DISCONTINUE_THREDHOLD         (3000 * 90)
#define DTV_PCRSCR_DEFAULT_LATENCY           (1000 * 90)
#define DTV_PCRSCR_MIN_LATENCY               (500 * 90)
#define DEMUX_PCR_DEFAULT_LATENCY            (500 * 90)
#define DEMUX_PCR_MIN_LATENCY                (300 * 90)
#define TIME_UNIT90K                         (1000 * 90)
#define DTV_AUDIO_MUTE_PRIOD_HTRESHOLD       (TIME_UNIT90K * 2)
#define DTV_AUDIO_DROP_TIMEOUT_HTRESHOLD     (TIME_UNIT90K)

#define DTV_AUDIO_JUMPED_THRESHOLD_PROPERTY   "vendor.media.audio.hal.dtv.jumped.threshold.ms"
#define DTV_AUDIO_RETUNE_THRESHOLD_PROPERTY   "vendor.media.audio.hal.dtv.retune.threshold.ms"

enum {
    AUDIO_DTV_PATCH_DECODER_STATE_INIT,
    AUDIO_DTV_PATCH_DECODER_STATE_START,
    AUDIO_DTV_PATCH_DECODER_STATE_RUNING,
    AUDIO_DTV_PATCH_DECODER_STATE_PAUSE,
    AUDIO_DTV_PATCH_DECODER_STATE_RESUME,
    AUDIO_DTV_PATCH_DECODER_RELEASE,
};

enum {
    AUDIO_DTV_PATCH_CMD_NULL,
    AUDIO_DTV_PATCH_CMD_START,
    AUDIO_DTV_PATCH_CMD_STOP,
    AUDIO_DTV_PATCH_CMD_PAUSE,
    AUDIO_DTV_PATCH_CMD_RESUME,
    AUDIO_DTV_PATCH_CMD_NUM,
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

/****below MS12 tunning is for dtv, ms*****/

/*speaker raw/pcm input*/
#define  DTV_AVSYNC_MS12_LATENCY_SPK_PCM             (50)
#define  DTV_AVSYNC_MS12_LATENCY_SPK_RAW             (50)
/*speaker dap enabled*/
#define  DTV_AVSYNC_MS12_LATENCY_DOLBY_DAP           (0)
/*atmos with arc*/
#define  DTV_AVSYNC_MS12_LATENCY_ARC_ATMOS           (180)
/*ddp sink with arc*/
#define  DTV_AVSYNC_MS12_LATENCY_ARC_ATMOSTODDP      (100)
#define  DTV_AVSYNC_MS12_LATENCY_ARC_RAWTODDP        (80)
#define  DTV_AVSYNC_MS12_LATENCY_ARC_PCMTODDP        (80)
/*dd sink with arc*/
#define  DTV_AVSYNC_MS12_LATENCY_ARC_RAWTODD         (60)
#define  DTV_AVSYNC_MS12_LATENCY_ARC_PCMTODD         (60)
/*pcm sink with arc*/
#define  DTV_AVSYNC_MS12_LATENCY_ARC_PCM              (30)
/*atmos passthrough with arc*/
#define  DTV_AVSYNC_MS12_LATENCY_ARC_ATMOS_PTH        (60)

/*above default tunning value is based DENON-X2400H AVR and the sound mode is music,
 *if the avr or mode is changed, there may be some small gaps in avsync,
 *may set below these props to correct it*/

/*speaker raw/pcm input*/
#define  DTV_AVSYNC_MS12_LATENCY_SPK_PCM_PROPERTY    "vendor.media.audio.hal.dtv.spk.pcm"
#define  DTV_AVSYNC_MS12_LATENCY_SPK_RAW_PROPERTY    "vendor.media.audio.hal.dtv.spk.raw"
#define  DTV_AVSYNC_MS12_LATENCY_DOLBY_DAP_PROPERTY  "vendor.media.audio.hal.dtv.dolby.dap"
/*atmos with arc*/
#define  DTV_AVSYNC_MS12_LATENCY_ARC_ATMOS_PROPERTY    "vendor.media.audio.hal.dtv.arc.atmos"
/*ddp sink with arc*/
#define  DTV_AVSYNC_MS12_LATENCY_ARC_ATMOSTODDP_PROPERTY    "vendor.media.audio.hal.dtv.arc.atmostoddp"
#define  DTV_AVSYNC_MS12_LATENCY_ARC_RAWTODDP_PROPERTY    "vendor.media.audio.hal.dtv.arc.rawtoddp"
#define  DTV_AVSYNC_MS12_LATENCY_ARC_PCMTODDP_PROPERTY    "vendor.media.audio.hal.dtv.arc.pcmtoddp"
/*dd sink with arc*/
#define  DTV_AVSYNC_MS12_LATENCY_ARC_RAWTODD_PROPERTY      "vendor.media.audio.hal.dtv.arc.rawtodd"
#define  DTV_AVSYNC_MS12_LATENCY_ARC_PCMTODD_PROPERTY      "vendor.media.audio.hal.dtv.arc.pcmtodd"
/*pcm sink with arc*/
#define  DTV_AVSYNC_MS12_LATENCY_ARC_PCM_PROPERTY      "vendor.media.audio.hal.dtv.arc.pcm"
/*atmos passthrough with arc*/
#define  DTV_AVSYNC_MS12_LATENCY_ARC_ATMOS_PTH_PROPERTY  "vendor.media.audio.hal.dtv.arc.pthatmos"
/*****************end of dtv tunning*************/

int create_dtv_patch(struct audio_hw_device *dev, audio_devices_t input, audio_devices_t output __unused);
int release_dtv_patch(struct aml_audio_device *dev);
int release_dtv_patch_l(struct aml_audio_device *dev);
int dtv_patch_add_cmd(int cmd);
int dtv_in_read(struct audio_stream_in *stream, void* buffer, size_t bytes);
void dtv_in_write(struct audio_stream_out *stream, const void* buffer, size_t bytes);
void save_latest_dtv_aformat(int afmt);
int audio_set_spdif_clock(struct aml_stream_out *stream,int type);

#define TSYNC_PCR_DEBUG_PTS    0x01
#define TSYNC_PCR_DEBUG_LOOKUP 0x40
#define TSYNC_PCR_DEBUG_AUDIO  0x20
#endif
