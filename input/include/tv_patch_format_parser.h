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


#ifndef __TV_PATCH_FORMAT_PARSER_H__
#define __TV_PATCH_FORMAT_PARSER_H__

#include <system/audio.h>
#include <tinyalsa/asoundlib.h>
#include <aml_alsa_mixer.h>
#include "aml_ac3_parser.h"

/*IEC61937 package presamble Pc value 0-4bit */
enum IEC61937_PC_Value {
    IEC61937_NULL               = 0x00,          ///< NULL data
    IEC61937_AC3                = 0x01,          ///< AC-3 data
    IEC61937_DTS1               = 0x0B,          ///< DTS type I   (512 samples)
    IEC61937_DTS2               = 0x0C,          ///< DTS type II  (1024 samples)
    IEC61937_DTS3               = 0x0D,          ///< DTS type III (2048 samples)
    IEC61937_DTSHD              = 0x11,          ///< DTS HD data
    IEC61937_EAC3               = 0x15,          ///< E-AC-3 data
    IEC61937_MAT                = 0x16,          ///< MAT data
    IEC61937_PAUSE              = 0x03,          ///< Pause
};

enum audio_type {
    LPCM = 0,
    AC3,
    EAC3,
    DTS,
    DTSHD,
    MAT,
    PAUSE,
    TRUEHD,
    DTSCD,
    MUTE,
    MPEGH,
};


enum audio_sample {
    HW_NONE = 0,
    HW_32K,
    HW_44K,
    HW_48K,
    HW_88K,
    HW_96K,
    HW_176K,
    HW_192K,
};

typedef enum hdmiin_audio_packet {
    AUDIO_PACKET_NONE,
    AUDIO_PACKET_AUDS,
    AUDIO_PACKET_OBA,
    AUDIO_PACKET_DST,
    AUDIO_PACKET_HBR,
    AUDIO_PACKET_OBM,
    AUDIO_PACKET_MAS
} hdmiin_audio_packet_t;

#define PARSER_DEFAULT_PERIOD_SIZE  (1024)

/*Period of data burst in IEC60958 frames*/
//#define AC3_PERIOD_SIZE  (6144)
//#define EAC3_PERIOD_SIZE (24576)
//#define MAT_PERIOD_SIZE  (61440)

#define DTS1_PERIOD_SIZE (2048)
#define DTS2_PERIOD_SIZE (4096)
#define DTS3_PERIOD_SIZE (8192)
/*min DTSHD Period 2048; max DTSHD Period 65536*/
#define DTSHD_PERIOD_SIZE   (512*8)
#define DTSHD_PERIOD_SIZE_1 (512*32)
#define DTSHD_PERIOD_SIZE_2 (512*48)

typedef struct audio_type_parse {
    struct pcm_config config_in;
    struct pcm *in;
    struct aml_mixer_handle *mixer_handle;
    unsigned int card;
    unsigned int device;
    unsigned int flags;
    int soft_parser;
    hdmiin_audio_packet_t hdmi_packet;
    hdmiin_audio_packet_t last_reconfig_hdmi_packet;

    int period_bytes;
    char *parse_buffer;

    int audio_type;
    int cur_audio_type;

    audio_channel_mask_t audio_ch_mask;

    int read_bytes;
    int package_size;
    int audio_samplerate;

    int running_flag;
    audio_devices_t input_dev;
} audio_type_parse_t;

int creat_pthread_for_audio_type_parse(
    pthread_t *audio_type_parse_ThreadID,
                     void **status,
                     struct aml_mixer_handle *mixer,
                     audio_devices_t input_dev);
void exit_pthread_for_audio_type_parse(
    pthread_t audio_type_parse_ThreadID,
    void **status);

/*
 *@brief convert the audio type to android audio format
 */
audio_format_t audio_type_convert_to_android_audio_format_t(int codec_type);

/*
 *@brief convert the audio type to string format
 */
char* audio_type_convert_to_string(int s32AudioType);

/*
 *@brief convert android audio format to the audio type
 */
int android_audio_format_t_convert_to_audio_type(audio_format_t format);
/*
 *@brief get current android audio format from audio parser thread
 */
audio_format_t audio_parse_get_audio_type(audio_type_parse_t *status);
/*
 *@brief get current audio channel mask from audio parser thread
 */
audio_channel_mask_t audio_parse_get_audio_channel_mask(audio_type_parse_t *status);
/*
 *@brief get current audio format from audio parser thread
 */
int audio_parse_get_audio_type_direct(audio_type_parse_t *status);
/*
 *@brief parse the channels in the undecoded DTS stream
 */
int get_dts_stream_channels(const char *buffer, size_t bytes);
/*
 *@brief get current audio type from buffer data
 */
int audio_type_parse(void *buffer, size_t bytes, int *package_size, audio_channel_mask_t *cur_ch_mask);

int audio_parse_get_audio_samplerate(audio_type_parse_t *status);

int eArcIn_audio_format_detection(struct aml_mixer_handle *mixer_handle);

int find_61937_sync_word(char *buffer, int size);

#endif
