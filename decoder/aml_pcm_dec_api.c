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

#define LOG_TAG "aml_audio_pcm_dec"

#include <cutils/log.h>
#include "aml_dec_api.h"
#include "aml_malloc_debug.h"
#include "audio_hw_utils.h"

#define PCM_MAX_LENGTH (8192*2*2)

struct pcm_dec_t {
    aml_dec_t  aml_dec;
    aml_dec_stream_info_t stream_info;
    int bit_rate;
    aml_pcm_config_t pcm_config;
};

static inline short CLIP16(int r)
{
    return (r >  0x7fff) ? 0x7fff :
           (r < -0x8000) ? -0x8000 :
           r;
}

static void downmix_4ch_to_2ch(void *in_buf, void *out_buf, int bytes, int audio_format) {
    int frames_num = 0;
    int channel = 4;
    int i = 0;
    if (audio_format == AUDIO_FORMAT_PCM_16_BIT) {
        frames_num = bytes / (channel * 2);
        int16_t *src = (int16_t *)in_buf;
        int16_t *dst = (int16_t *)out_buf;
        for (i = 0; i < frames_num; i++) {
            dst[2*i]   = (int16_t)CLIP16((int)src[channel*i] * 0.5
                       + (int)src[channel*i + 2] * 0.25
                       + (int)src[channel*i + 3] * 0.25) ;
            dst[2*i+1] = (int16_t)CLIP16((int)src[channel*i + 1] * 0.5
                       + (int)src[channel*i + 2] * 0.25
                       + (int)src[channel*i + 3] * 0.25);
        }
    }
    return;
}

static void downmix_6ch_to_2ch(void *in_buf, void *out_buf, int bytes, int audio_format) {
    int frames_num = 0;
    int channel = 6;
    int i = 0;
    if (audio_format == AUDIO_FORMAT_PCM_16_BIT) {
        frames_num = bytes / (channel * 2);
        int16_t *src = (int16_t *)in_buf;
        int16_t *dst = (int16_t *)out_buf;
        for (i = 0; i < frames_num; i++) {
            dst[2*i]   = (int16_t)CLIP16((int)src[channel*i] * 0.5
                       + (int)src[channel*i + 2] * 0.25
                       + (int)src[channel*i + 3] * 0.25
                       + (int)src[channel*i + 5] * 0.25);
            dst[2*i+1] = (int16_t)CLIP16((int)src[channel*i + 1] * 0.5
                       + (int)src[channel*i + 2] * 0.25
                       + (int)src[channel*i + 4] * 0.25
                       + (int)src[channel*i + 5] * 0.25);
        }
    }
    return;
}

static void downmix_8ch_to_2ch(void *in_buf, void *out_buf, int bytes, int audio_format) {
    int frames_num = 0;
    int channel = 8;
    int i = 0;
    if (audio_format == AUDIO_FORMAT_PCM_16_BIT) {
        frames_num = bytes / (channel * 2);
        int16_t *src = (int16_t *)in_buf;
        int16_t *dst = (int16_t *)out_buf;
        for (i = 0; i < frames_num; i++) {
            dst[2*i]   = (int16_t)CLIP16((int)src[channel*i] * 0.5
                       + (int)src[channel*i + 2] * 0.25
                       + (int)src[channel*i + 3] * 0.25
                       + (int)src[channel*i + 4] * 0.25
                       + (int)src[channel*i + 6] * 0.25) ;
            dst[2*i+1] = (int16_t)CLIP16((int)src[channel*i + 1] * 0.5
                       + (int)src[channel*i + 2] * 0.25
                       + (int)src[channel*i + 3] * 0.25
                       + (int)src[channel*i + 5] * 0.25
                       + (int)src[channel*i + 7] * 0.25);
        }
    }
    return;
}


static int pcm_decoder_init(aml_dec_t **ppaml_dec, aml_dec_config_t * dec_config)
{
    struct pcm_dec_t *pcm_dec;
    aml_dec_t  *aml_dec = NULL;
    aml_pcm_config_t *pcm_config = NULL;
    dec_data_info_t * dec_pcm_data = NULL;
    dec_data_info_t * raw_in_data = NULL;

    if (dec_config == NULL) {
        ALOGE("PCM config is NULL\n");
        return -1;
    }
    pcm_config = &dec_config->pcm_config;

    if (pcm_config->channel <= 0 || pcm_config->channel > 8) {
        ALOGE("PCM config channel is invalid=%d\n", pcm_config->channel);
        return -1;
    }

    if (pcm_config->samplerate <= 0 || pcm_config->samplerate > 192000) {
        ALOGE("PCM config samplerate is invalid=%d\n", pcm_config->samplerate);
        return -1;
    }

    if (!audio_is_linear_pcm(pcm_config->pcm_format)) {
        ALOGE("PCM config format is not supported =%d\n", pcm_config->pcm_format);
        return -1;
    }

    pcm_dec = aml_audio_calloc(1, sizeof(struct pcm_dec_t));
    if (pcm_dec == NULL) {
        ALOGE("malloc ddp_dec failed\n");
        return -1;
    }

    aml_dec = &pcm_dec->aml_dec;
    memcpy(&pcm_dec->pcm_config, pcm_config, sizeof(aml_pcm_config_t));

    dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_pcm_data->buf_size = PCM_MAX_LENGTH;
    dec_pcm_data->buf = (unsigned char*) aml_audio_calloc(1, dec_pcm_data->buf_size);
    if (!dec_pcm_data->buf) {
        ALOGE("malloc buffer failed\n");
        goto exit;
    }

    raw_in_data = &aml_dec->raw_in_data;
    raw_in_data->buf_size = PCM_MAX_LENGTH;
    raw_in_data->buf = (unsigned char*) aml_audio_calloc(1, raw_in_data->buf_size);
    if (!raw_in_data->buf) {
        ALOGE("malloc buffer failed\n");
        return -1;
    }

    memset(&(pcm_dec->stream_info), 0x00, sizeof(pcm_dec->stream_info));

    aml_dec->status = 1;
    *ppaml_dec = (aml_dec_t *)pcm_dec;
    ALOGI("[%s:%d] success PCM format=%d, samplerate:%d, ch:%d", __func__, __LINE__,
        pcm_config->pcm_format, pcm_config->samplerate, pcm_config->channel);
    return 0;

exit:
    if (pcm_dec) {
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }
        if (raw_in_data->buf) {
            aml_audio_free(raw_in_data->buf);
        }
        aml_audio_free(pcm_dec);
    }
    *ppaml_dec = NULL;
    ALOGE("%s failed", __func__);
    return -1;
}

static int pcm_decoder_release(aml_dec_t * aml_dec)
{
    dec_data_info_t * dec_pcm_data = NULL;
    dec_data_info_t * raw_in_data = NULL;

    if (aml_dec != NULL) {
        dec_pcm_data = &aml_dec->dec_pcm_data;
        raw_in_data = &aml_dec->raw_in_data;
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }
        if (raw_in_data->buf) {
            aml_audio_free(raw_in_data->buf);
        }

        aml_audio_free(aml_dec);
    }
    return 0;
}

static int parse_lpcm_bluray_header (aml_pcm_config_t *pcm_config, unsigned int header)
{
  int ret = 0;
  if (header != pcm_config->lpcm_header) {
    pcm_config->lpcm_header = header;
    switch ((header >> 6) & 0x3) {
      case 0x1:
        pcm_config->width = 16;
        break;
      case 0x2:
        pcm_config->width = 20;
        break;
      case 0x3:
        pcm_config->width = 24;
        break;
      default:
        pcm_config->width = 16;
        ALOGE ("%s():%d Invalid sample depth!", __FUNCTION__, __LINE__);
        ret = -1;
        break;
    }
    switch ((header >> 8) & 0xf) {
      case 0x1:
        pcm_config->samplerate = 48000;
        break;
      case 0x4:
        pcm_config->samplerate = 96000;
        break;
      case 0x5:
        pcm_config->samplerate = 192000;
        break;
      default:
        pcm_config->samplerate = 48000;
        ALOGE ("%s():%d Invalid audio sampling frequency!", __FUNCTION__, __LINE__);
        ret = -1;
        break;
    }
    switch ((header >> 12) & 0xf) {
      case 0x1:
        pcm_config->lpcm_channel = 1;
        break;
      case 0x3:
        pcm_config->lpcm_channel = 2;
        break;
      case 0x4:
      case 0x5:
        pcm_config->lpcm_channel = 3;
        break;
      case 0x6:
      case 0x7:
        pcm_config->lpcm_channel = 4;
        break;
      case 0x8:
        pcm_config->lpcm_channel = 5;
        break;
      case 0x9:
        pcm_config->lpcm_channel = 6;
        break;
      case 0xa:
        pcm_config->lpcm_channel = 7;
        break;
      case 0xb:
        pcm_config->lpcm_channel = 8;
        break;
      default:
        pcm_config->lpcm_channel = 2;
        ALOGE ("%s():%d Invalid number of audio channels!", __FUNCTION__, __LINE__);
        ret = -1;
    }
    ALOGI("%s:%d width(%d), samplerate(%d), channel(%d)", __FUNCTION__, __LINE__,
    pcm_config->width, pcm_config->samplerate, pcm_config->lpcm_channel);
  }
  return ret;
}
static int parse_lpcm_dvd_header (aml_pcm_config_t *pcm_config, unsigned int header)
{
  int ret = 0;
  if (header != pcm_config->lpcm_header) {
    pcm_config->lpcm_header = header;
    switch (header & 0xC000) {
      case 0x8000:
        pcm_config->width = 24;
        break;
      case 0x4000:
        pcm_config->width = 20;
        break;
      default:
        pcm_config->width = 16;
        break;
    }
    switch (header & 0x3000) {
      case 0x0000:
        pcm_config->samplerate = 48000;
        break;
      case 0x1000:
        pcm_config->samplerate = 96000;
        break;
      case 0x2000:
        pcm_config->samplerate = 44100;
        break;
      case 0x3000:
        pcm_config->samplerate = 32000;
        break;
      default:
        pcm_config->samplerate = 48000;
        ALOGE ("%s():%d Invalid audio sampling frequency!", __FUNCTION__, __LINE__);
        ret = -1;
        break;
    }
  }
  pcm_config->lpcm_channel = ((header >> 8) & 0x7) + 1;
  if (pcm_config->lpcm_channel < 1 || pcm_config->lpcm_channel > 8) {
    ALOGE ("%s():%d Invalid audio channel(%d)!", __FUNCTION__, __LINE__, pcm_config->lpcm_channel);
    pcm_config->lpcm_channel = 2;
  }
  return ret;
}
static int parse_lpcm_1394_header (aml_pcm_config_t *pcm_config, unsigned int header)
{
  int ret = 0;
  if (header != pcm_config->lpcm_header) {
    if (header >> 24 != 0xa0) {
      ALOGE ("%s():%d Invalid audio data!", __FUNCTION__, __LINE__);
    }
    pcm_config->lpcm_header = header;
    switch ((header >> 6) & 0x3) {
      case 0x0:
        pcm_config->width = 16;
        break;
      default:
        pcm_config->width = 16;
        ret = -1;
        ALOGE ("%s():%d Invalid quantization word length!", __FUNCTION__, __LINE__);
        break;
    }
    switch ((header >> 3) & 0x7) {
      case 0x1:
        pcm_config->samplerate = 44100;
        break;
      case 0x2:
        pcm_config->samplerate = 48000;
        break;
      default:
        pcm_config->samplerate = 48000;
        ret = -1;
        ALOGE ("%s():%d Invalid audio sampling frequency!", __FUNCTION__, __LINE__);
        break;
    }
    switch (header & 0x7) {
      case 0x0:                /* 2 channels dual-mono */
      case 0x1:                /* 2 channels stereo */
        pcm_config->lpcm_channel = 2;
        break;
      default:
        pcm_config->lpcm_channel = 2;
        ret = -1;
        ALOGE ("%s():%d Invalid number of audio channels!", __FUNCTION__, __LINE__);
    }
  }
  return ret;
}
static inline int16_t av_clip_int16(int a)
{
    if ((a + 32768) & ~65535) {
        return (a >> 31) ^ 32767;
    } else {
        return a;
    }
}
static aml_dec_return_type_t convert_data_from_be_to_16bit_le(aml_dec_t * aml_dec, const char *buffer, int in_bytes) {
    dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
    struct pcm_dec_t *pcm_dec = NULL;
    aml_pcm_config_t* pcm_config = NULL;
    pcm_dec = (struct pcm_dec_t *)aml_dec;
    pcm_config = &pcm_dec->pcm_config;
    if (dec_pcm_data->buf_size < in_bytes * 2) { // the max out buffer size is that input is 1ch 16bit
        ALOGI("%s() realloc outbuf_max_len  from %zu to %zu\n", __FUNCTION__, dec_pcm_data->buf_size, in_bytes * 2);
        dec_pcm_data->buf = aml_audio_realloc(dec_pcm_data->buf, in_bytes * 2);
        if (dec_pcm_data->buf == NULL) {
            ALOGE("%s() realloc pcm buffer failed size %zu\n", __FUNCTION__, in_bytes * 2);
            return AML_DEC_RETURN_TYPE_FAIL;
        }
        dec_pcm_data->buf_size = in_bytes * 2;
        memset(dec_pcm_data->buf, 0, dec_pcm_data->buf_size);
    }
    int i, j, m;
    int tmp_buf[10] = {0};
    int frame_num = 0;
    short* sample = (short *)dec_pcm_data->buf;
    if (pcm_config->width  == 16) {
        frame_num = in_bytes / pcm_config->lpcm_channel / 2;
        if (pcm_config->lpcm_channel == 1) {
            for (i = 0, j = 0, m = 0; m < frame_num; m++) {
                sample[j + 1] = sample[j] = (buffer[i] << 8) | buffer[i + 1];
                i += 2;
                j += 2;
            }
        } else if (pcm_config->lpcm_channel == 2) {
            for (i = 0, j = 0, m = 0; m < frame_num; m++) {
                sample[j++] = (buffer[i] << 8) | buffer[i + 1];
                i += 2;
                sample[j++] = (buffer[i] << 8) | buffer[i + 1];
                i += 2;
            }
        } else if (pcm_config->lpcm_channel > 2 && pcm_config->lpcm_channel <= 8) {
            int k;
            memset(tmp_buf, 0, sizeof(tmp_buf));
            for (i = 0, j = 0, m = 0; m < frame_num; m++) {
                for (k = 0; k < pcm_config->lpcm_channel; k++) {
                    tmp_buf[k] = (int16_t)((buffer[i] << 8) | buffer[i + 1]);
                    i += 2;
                }
                sample[j++] = av_clip_int16(tmp_buf[0] + 0.707f * tmp_buf[2] + 0.707f * (tmp_buf[3] + tmp_buf[6])); //Lo=L+0.707C+0.707 (Ls+Lsr)
                sample[j++] = av_clip_int16(tmp_buf[1] + 0.707f * tmp_buf[2] + 0.707f * (tmp_buf[4] + tmp_buf[7])); //Ro=R+0.707C+0.707(Rs+Rsr)
            }
        }
    } else if (pcm_config->width  == 24 || pcm_config->width  == 20) {
        int k;
        memset(tmp_buf, 0, sizeof(tmp_buf));
        frame_num = in_bytes / pcm_config->lpcm_channel / 3;
        if (pcm_config->lpcm_channel == 1) {
            for (i = 0, j = 0, m = 0; m < frame_num; m++) {
                sample[j++] = (buffer[i] << 8) | buffer[i + 1];
                i += 3;
            }
        } else if (pcm_config->lpcm_channel == 2) {
            for (i = 0, j = 0, m = 0; m < frame_num; m++) {
                sample[j++] = (buffer[i] << 8) | buffer[i + 1];
                i += 3;
                sample[j++] = (buffer[i] << 8) | buffer[i + 1];
                i += 3;
            }
        } else if (pcm_config->lpcm_channel >= 2 && pcm_config->lpcm_channel <= 8) {
            int k;
            memset(tmp_buf, 0, sizeof(tmp_buf));
            for (i = 0, j = 0, m = 0; m < frame_num; m++) {
                for (k = 0; k < pcm_config->lpcm_channel; k++) {
                    tmp_buf[k] = (int16_t)(buffer[i] << 8) | buffer[i + 1];
                    i += 3;
                }
                sample[j++] = av_clip_int16(tmp_buf[0] + 0.707f * tmp_buf[2] + 0.707f * (tmp_buf[3] + tmp_buf[6])); //Lo=L+0.707C+0.707 (Ls+Lsr)
                sample[j++] = av_clip_int16(tmp_buf[1] + 0.707f * tmp_buf[2] + 0.707f * (tmp_buf[4] + tmp_buf[7])); //Ro=R+0.707C+0.707(Rs+Rsr)
            }
        }
    } else {
        ALOGE("[%s %d]lpcm is %d bps, don't process now\n", __FUNCTION__, __LINE__, pcm_config->width);
        return 0;
    }
    dec_pcm_data->data_len = frame_num * 4;
    return AML_DEC_RETURN_TYPE_OK;
}
static aml_dec_return_type_t lpcm_process(aml_dec_t * aml_dec, const char *buffer, int in_bytes)
{
    int header_size = 0;
    unsigned int header = 0;
    int bytes = in_bytes;
    struct pcm_dec_t *pcm_dec = NULL;
    aml_pcm_config_t* pcm_config = NULL;
    pcm_dec = (struct pcm_dec_t *)aml_dec;
    pcm_config = &pcm_dec->pcm_config;
    if (pcm_config->pcm_format == AUDIO_FORMAT_PCM_LPCM_BLURAY) {
        header = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
        header_size = 4;
        if (-1 == parse_lpcm_bluray_header (pcm_config, header)) {
            ALOGE ("%s():%d AUDIO_FORMAT_PCM_LPCM_BLURAY parser header fail!", __FUNCTION__, __LINE__);
        }
    } else if (pcm_config->pcm_format == AUDIO_FORMAT_PCM_LPCM_DVD) {
        int first_access = (buffer[0] << 8) | buffer[1];
        if (first_access > bytes) {
            ALOGE ("%s():%d invalid data!", __FUNCTION__, __LINE__);
        }
        header = ((buffer[2] & 0xC0) << 16) | (buffer[3] << 8) | buffer[4];
        header_size = 5;
        if (-1 == parse_lpcm_dvd_header (pcm_config, header)) {
            ALOGE ("%s():%d AUDIO_FORMAT_PCM_LPCM_DVD parser header fail!", __FUNCTION__, __LINE__);
        }
    } else if (pcm_config->pcm_format == AUDIO_FORMAT_PCM_LPCM_1394) {
        header = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
        header_size = 4;
        if (-1 == parse_lpcm_1394_header (pcm_config, header)) {
            ALOGE ("%s():%d AUDIO_FORMAT_PCM_LPCM_1394 parser header fail!", __FUNCTION__, __LINE__);
        }
    }
    return convert_data_from_be_to_16bit_le(aml_dec, buffer + header_size, bytes - header_size);
}

static bool is_lpcm(audio_format_t format)
{
    if (format == AUDIO_FORMAT_PCM_LPCM_BLURAY|| format == AUDIO_FORMAT_PCM_LPCM_DVD || format == AUDIO_FORMAT_PCM_LPCM_1394) {
        return true;
    } else {
        return false;
    }
}

static bool is_u8pcm(audio_format_t format)
{
    if (format == AUDIO_FORMAT_PCM_8_BIT) {
        return true;
    } else {
        return false;
    }
}

static aml_dec_return_type_t u8pcm_process(aml_dec_t * aml_dec, const char*buffer, int in_bytes)
{
    short *ouBuffer = NULL;
    dec_data_info_t *dec_pcm_data = &aml_dec->dec_pcm_data;

    if (dec_pcm_data->buf_size < in_bytes * 2) { // the max out buffer size is that input is 1ch 16bit
        ALOGI("%s() realloc outbuf_max_len from %zu to %zu\n", __FUNCTION__, dec_pcm_data->buf_size, in_bytes * 2);
        dec_pcm_data->buf = aml_audio_realloc(dec_pcm_data->buf, in_bytes * 2);
        if (dec_pcm_data->buf == NULL) {
            ALOGE("%s() realloc pcm buffer failed size %zu\n", __FUNCTION__, in_bytes * 2);
            return AML_DEC_RETURN_TYPE_FAIL;
        }
        dec_pcm_data->buf_size = in_bytes * 2;
        memset(dec_pcm_data->buf, 0, dec_pcm_data->buf_size);
    }

    ouBuffer = (short *)dec_pcm_data->buf;
    for (int i = 0; i < in_bytes; i++) {
        ouBuffer[i] = (short)(buffer[i] - 128)*256;
    }

    dec_pcm_data->data_len = in_bytes * 2;
    return AML_DEC_RETURN_TYPE_OK;
}

static void pcm32bit_to_16bit (void *in_buf, void *out_buf, int bytes, int audio_format)
{
    if (audio_format == AUDIO_FORMAT_PCM_32_BIT) {
        int32_t *input32 = (int32_t *)in_buf;
        int16_t *output16 = (int16_t *)out_buf;
        int i = 0;
        for (i = 0; i < bytes / sizeof(int32_t); i++) {
            output16[i] = (int16_t)((input32[i] >> 16) & 0xffff);
        }
    }
    return;
}

static int pcm_decoder_process(aml_dec_t * aml_dec, struct audio_buffer *abuffer)
{
    struct pcm_dec_t *pcm_dec = NULL;
    aml_pcm_config_t *pcm_config = NULL;
    const char *buffer = abuffer->buffer;
    int bytes = abuffer->size;
    int in_bytes = bytes;
    int src_channel = 0;
    int dst_channel = 0;
    int downmix_conf = 1;
    int downmix_size = 0;

    if (aml_dec == NULL) {
        ALOGE("%s aml_dec is NULL", __func__);
        return AML_DEC_RETURN_TYPE_FAIL;
    }

    if (bytes <= 0) {
        return AML_DEC_RETURN_TYPE_FAIL;
    }

    pcm_dec = (struct pcm_dec_t *)aml_dec;
    pcm_config = &pcm_dec->pcm_config;
    dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t * raw_in_data = &aml_dec->raw_in_data;

    dec_pcm_data->data_len = 0;
    raw_in_data->data_len = 0;

    if (is_lpcm(pcm_config->pcm_format) == true) {
        if (lpcm_process(aml_dec, buffer, bytes) != AML_DEC_RETURN_TYPE_OK) {
            ALOGE("%s():%d lpcm_process error", __FUNCTION__, __LINE__);
            return in_bytes;
        }
        pcm_config->channel = 2;
        src_channel = pcm_config->lpcm_channel;
    } else if (is_u8pcm(pcm_config->pcm_format)) {
        if (u8pcm_process(aml_dec, buffer, bytes) != AML_DEC_RETURN_TYPE_OK) {
            ALOGE("%s():%d u8pcm_process error", __FUNCTION__, __LINE__);
            return in_bytes;
        }
        src_channel = pcm_config->channel;
    } else {
        src_channel = pcm_config->channel;
        dst_channel = 2;
        downmix_conf = src_channel / dst_channel;
        downmix_size = bytes / downmix_conf;

        if (dec_pcm_data->buf_size < downmix_size) {
            ALOGI("realloc outbuf_max_len  from %zu to %zu\n", dec_pcm_data->buf_size, downmix_size);
            dec_pcm_data->buf = aml_audio_realloc(dec_pcm_data->buf, downmix_size);
            if (dec_pcm_data->buf == NULL) {
                ALOGE("realloc pcm buffer failed size %zu\n", downmix_size);
                return AML_DEC_RETURN_TYPE_FAIL;
            }
            dec_pcm_data->buf_size = downmix_size;
            memset(dec_pcm_data->buf, 0, downmix_size);
        }


        if (pcm_config->channel == 2) {
            if (pcm_config->pcm_format == AUDIO_FORMAT_PCM_32_BIT) {
                pcm32bit_to_16bit(buffer, dec_pcm_data->buf, bytes, pcm_config->pcm_format);
                downmix_size = downmix_size / 2;
            } else {
                /*now we only support bypass PCM data*/
                memcpy(dec_pcm_data->buf, buffer, bytes);
            }
        } else if (pcm_config->channel == 4) {
            downmix_4ch_to_2ch(buffer, dec_pcm_data->buf, bytes, pcm_config->pcm_format);
        } else if (pcm_config->channel == 6) {
            downmix_6ch_to_2ch(buffer, dec_pcm_data->buf, bytes, pcm_config->pcm_format);
        } else if (pcm_config->channel == 8) {
            downmix_8ch_to_2ch(buffer, dec_pcm_data->buf, bytes, pcm_config->pcm_format);
        } else {
            ALOGI("unsupport channel =%d", pcm_config->channel);
            return AML_DEC_RETURN_TYPE_OK;
        }

        dec_pcm_data->data_len = downmix_size;
    }
    pcm_dec->stream_info.stream_decode_num += dec_pcm_data->data_len/4;
    if (pcm_dec->stream_info.stream_bitrate == 0) {
        pcm_dec->stream_info.stream_bitrate = src_channel*2*pcm_config->samplerate;
    }
    pcm_dec->stream_info.stream_ch = src_channel;
    pcm_dec->stream_info.stream_sr = pcm_config->samplerate;
    dec_pcm_data->data_sr = pcm_config->samplerate;
    dec_pcm_data->data_ch = 2;
    dec_pcm_data->data_format = pcm_config->pcm_format;
    ALOGV("%s data_in=%d ch =%d out=%d ch=%d", __func__, bytes, pcm_config->channel, downmix_size, 2);

    if (pcm_config->max_out_channels >= pcm_config->channel) {
        if (raw_in_data->buf_size < bytes) {
            ALOGI("realloc outbuf_max_len  from %zu to %zu\n", raw_in_data->buf_size, bytes);
            raw_in_data->buf = aml_audio_realloc(raw_in_data->buf, bytes);
            if (raw_in_data->buf == NULL) {
                ALOGE("realloc pcm buffer failed size %zu\n", bytes);
                return AML_DEC_RETURN_TYPE_FAIL;
            }
            raw_in_data->buf_size = bytes;
            memset(raw_in_data->buf, 0, bytes);
        }
        memcpy(raw_in_data->buf, buffer, bytes);
        raw_in_data->data_len = bytes ;
        raw_in_data->data_sr  = pcm_config->samplerate;
        raw_in_data->data_ch  = pcm_config->channel;
        raw_in_data->data_format  = pcm_config->pcm_format;
        ALOGV("%s multi data_in=%d ch =%d out=%d ch=%d", __func__, bytes, pcm_config->channel, downmix_size, pcm_config->channel);
    }

    dec_pcm_data->pts = abuffer->pts;

    AM_LOGI_IF(aml_dec->debug_level, "pts: 0x%llx (%lld ms) pcm len %d, buffer len %d, used_size_return %d",
        dec_pcm_data->pts, dec_pcm_data->pts/90, dec_pcm_data->data_len, dec_pcm_data->buf_size, in_bytes);

    return in_bytes;
}

static int pcm_decoder_getinfo(aml_dec_t *aml_dec, aml_dec_info_type_t info_type, aml_dec_info_t * dec_info)
{
    int ret = -1;
    struct pcm_dec_t *pcm_dec= (struct pcm_dec_t *)aml_dec;

    switch (info_type) {
    case AML_DEC_REMAIN_SIZE:
        //dec_info->remain_size = pcm_dec->remain_size;
        return 0;
    case AML_DEC_STREMAM_INFO:
        memset(&dec_info->dec_info, 0x00, sizeof(aml_dec_stream_info_t));
        memcpy(&dec_info->dec_info, &pcm_dec->stream_info, sizeof(aml_dec_stream_info_t));
        return 0;
    default:
        break;
    }
    return ret;
}

aml_dec_func_t aml_pcm_func = {
    .f_init                 = pcm_decoder_init,
    .f_release              = pcm_decoder_release,
    .f_process              = pcm_decoder_process,
    .f_config               = NULL,
    .f_info                 = pcm_decoder_getinfo,
};
