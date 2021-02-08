/*
 * dmx.h
 *
 * Copyright (C) 2000 Marcus Metzler <marcus@convergence.de>
 *                  & Ralph  Metzler <ralph@convergence.de>
 *                    for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef _UAPI_DVBDMX_H_
#define _UAPI_DVBDMX_H_

#include <linux/types.h>
#ifndef __KERNEL__
#include <time.h>
#endif

/*24~31 one byte for audio type, dmx_audio_format_t*/
#define DMX_AUDIO_FORMAT_BIT 24

#define CONFIG_AMLOGIC_DVB_COMPAT

#define DMX_FILTER_SIZE 16

enum dmx_output
{
	DMX_OUT_DECODER, /* Streaming directly to decoder. */
	DMX_OUT_TAP,     /* Output going to a memory buffer */
			 /* (to be retrieved via the read command).*/
	DMX_OUT_TS_TAP,  /* Output multiplexed into a new TS  */
			 /* (to be retrieved by reading from the */
			 /* logical DVR device).                 */
	DMX_OUT_TSDEMUX_TAP /* Like TS_TAP but retrieved from the DMX device */
};

typedef enum dmx_output dmx_output_t;

typedef enum dmx_input
{
	DMX_IN_FRONTEND, /* Input from a front-end device.  */
	DMX_IN_DVR       /* Input from the logical DVR device.  */
} dmx_input_t;

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
#define ACODEC_FMT_MPEG1                                                       \
  26 // AFORMAT_MPEG-->mp3,AFORMAT_MPEG1-->mp1,AFROMAT_MPEG2-->mp2
#define ACODEC_FMT_MPEG2 27
#define ACODEC_FMT_WMAVOI 28

typedef enum dmx_ts_pes
{
	DMX_PES_AUDIO0,
	DMX_PES_VIDEO0,
	DMX_PES_TELETEXT0,
	DMX_PES_SUBTITLE0,
	DMX_PES_PCR0,

	DMX_PES_AUDIO1,
	DMX_PES_VIDEO1,
	DMX_PES_TELETEXT1,
	DMX_PES_SUBTITLE1,
	DMX_PES_PCR1,

	DMX_PES_AUDIO2,
	DMX_PES_VIDEO2,
	DMX_PES_TELETEXT2,
	DMX_PES_SUBTITLE2,
	DMX_PES_PCR2,

	DMX_PES_AUDIO3,
	DMX_PES_VIDEO3,
	DMX_PES_TELETEXT3,
	DMX_PES_SUBTITLE3,
	DMX_PES_PCR3,

	DMX_PES_OTHER
} dmx_pes_type_t;

#define DMX_PES_AUDIO    DMX_PES_AUDIO0
#define DMX_PES_VIDEO    DMX_PES_VIDEO0
#define DMX_PES_TELETEXT DMX_PES_TELETEXT0
#define DMX_PES_SUBTITLE DMX_PES_SUBTITLE0
#define DMX_PES_PCR      DMX_PES_PCR0


typedef struct dmx_filter
{
	__u8  filter[DMX_FILTER_SIZE];
	__u8  mask[DMX_FILTER_SIZE];
	__u8  mode[DMX_FILTER_SIZE];
} dmx_filter_t;


struct dmx_sct_filter_params
{
	__u16          pid;
	dmx_filter_t   filter;
	__u32          timeout;
	__u32          flags;
#define DMX_CHECK_CRC       1
#define DMX_ONESHOT         2
#define DMX_IMMEDIATE_START 4
#define DMX_KERNEL_CLIENT   0x8000

#ifdef CONFIG_AMLOGIC_DVB_COMPAT
#define DMX_USE_SWFILTER    0x100
#endif
};

#ifdef CONFIG_AMLOGIC_DVB_COMPAT

typedef enum dmx_input_source {
	INPUT_DEMOD,
	INPUT_LOCAL,
	INPUT_LOCAL_SEC
} dmx_input_source_t;

/**
 * struct dmx_non_sec_es_header - non-sec Elementary Stream (ES) Header
 *
 * @pts_dts_flag:[1:0], 01:pts valid, 10:dts valid
 * @pts:	pts value
 * @dts:	dts value
 * @len:	data len
 */
struct dmx_non_sec_es_header {
	__u8 pts_dts_flag;
	__u64 pts;
	__u64 dts;
	__u32 len;
};

/**
 * struct dmx_sec_es_data - sec Elementary Stream (ES)
 *
 * @pts_dts_flag:[1:0], 01:pts valid, 10:dts valid
 * @pts:	pts value
 * @dts:	dts value
 * @buf_start:	buf start addr
 * @buf_end:	buf end addr
 * @data_start: data start addr
 * @data_end: data end addr
 */
struct dmx_sec_es_data {
	__u8 pts_dts_flag;
	__u64 pts;
	__u64 dts;
	__u32 buf_start;
	__u32 buf_end;
	__u32 data_start;
	__u32 data_end;
};
#endif
enum dmx_audio_format {
        AUDIO_UNKNOWN = 0,      /* unknown media */
        AUDIO_MPX = 1,          /* mpeg audio MP2/MP3 */
        AUDIO_AC3 = 2,          /* Dolby AC3/EAC3 */
        AUDIO_AAC_ADTS = 3,     /* AAC-ADTS */
        AUDIO_AAC_LOAS = 4,     /* AAC-LOAS */
        AUDIO_DTS = 5,          /* DTS */
        AUDIO_MAX
};

struct dmx_pes_filter_params
{
	__u16          pid;
	dmx_input_t    input;
	dmx_output_t   output;
	dmx_pes_type_t pes_type;
	__u32          flags;
    #ifdef CONFIG_AMLOGIC_DVB_COMPAT
#define DMX_MEM_SEC_LEVEL1   (1 << 10)
#define DMX_MEM_SEC_LEVEL2   (1 << 11)
#define DMX_MEM_SEC_LEVEL3   (1 << 12)

#define DMX_ES_OUTPUT        (1 << 16)
#define DMX_OUTPUT_RAW_MODE       (1 << 17)

    #endif
};

typedef struct dmx_caps {
	__u32 caps;
	int num_decoders;
} dmx_caps_t;

typedef enum dmx_source {
	DMX_SOURCE_FRONT0 = 0,
	DMX_SOURCE_FRONT1,
	DMX_SOURCE_FRONT2,
	DMX_SOURCE_FRONT3,
	DMX_SOURCE_DVR0   = 16,
	DMX_SOURCE_DVR1,
	DMX_SOURCE_DVR2,
	DMX_SOURCE_DVR3,

#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	DMX_SOURCE_FRONT0_OFFSET = 100,
	DMX_SOURCE_FRONT1_OFFSET,
	DMX_SOURCE_FRONT2_OFFSET
#endif
} dmx_source_t;

struct dmx_stc {
	unsigned int num;	/* input : which STC? 0..N */
	unsigned int base;	/* output: divisor for stc to get 90 kHz clock */
	__u64 stc;		/* output: stc in 'base'*90 kHz units */
};

#define DMX_START                _IO('o', 41)
#define DMX_STOP                 _IO('o', 42)
#define DMX_SET_FILTER           _IOW('o', 43, struct dmx_sct_filter_params)
#define DMX_SET_PES_FILTER       _IOW('o', 44, struct dmx_pes_filter_params)
#define DMX_SET_BUFFER_SIZE      _IO('o', 45)
#define DMX_GET_PES_PIDS         _IOR('o', 47, __u16[5])
#define DMX_GET_CAPS             _IOR('o', 48, dmx_caps_t)
#define DMX_SET_SOURCE           _IOW('o', 49, dmx_source_t)
#define DMX_GET_STC              _IOWR('o', 50, struct dmx_stc)
#define DMX_ADD_PID              _IOW('o', 51, __u16)
#define DMX_REMOVE_PID           _IOW('o', 52, __u16)
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
#define DMX_SET_INPUT           _IO('o', 80)
#endif

#endif /* _UAPI_DVBDMX_H_ */
