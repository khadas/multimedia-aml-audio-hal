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

#ifndef AMDMX_H
#define AMDMX_H

#include <stdio.h>
#include <string.h>
#include <am_types.h>
#include <linux/types.h>
#include <pthread.h>
#include "RefBase.h"
#include <AmHwMultiDemuxWrapper.h>

namespace audio_dmx {

#define DMX_FILTER_COUNT      (32)

#define DMX_FL_RUN_CB         (1)

#define DMX_BUF_SIZE       (4096)
#define DMX_POLL_TIMEOUT   (200)
#define DMX_DEV_COUNT      (32)

#define DMX_CHAN_ISSET_FILTER(chan,fid)    ((chan)->filter_mask[(fid)>>3]&(1<<((fid)&3)))
#define DMX_CHAN_SET_FILTER(chan,fid)      ((chan)->filter_mask[(fid)>>3]|=(1<<((fid)&3)))
#define DMX_CHAN_CLR_FILTER(chan,fid)      ((chan)->filter_mask[(fid)>>3]&=~(1<<((fid)&3)))


enum AM_DMX_ErrorCode
{
	AM_DMX_ERROR_BASE=AM_ERROR_BASE(AM_MOD_DMX),
	AM_DMX_ERR_INVALID_DEV_NO,          /**< Invalid device number*/
	AM_DMX_ERR_INVALID_ID,              /**< Invalid filer handle*/
	AM_DMX_ERR_BUSY,                    /**< The device has already been openned*/
	AM_DMX_ERR_NOT_ALLOCATED,           /**< The device has not been allocated*/
	AM_DMX_ERR_CANNOT_CREATE_THREAD,    /**< Cannot create new thread*/
	AM_DMX_ERR_CANNOT_OPEN_DEV,         /**< Cannot open device*/
	AM_DMX_ERR_NOT_SUPPORTED,           /**< Not supported*/
	AM_DMX_ERR_NO_FREE_FILTER,          /**< No free filter*/
	AM_DMX_ERR_NO_MEM,                  /**< Not enough memory*/
	AM_DMX_ERR_TIMEOUT,                 /**< Timeout*/
	AM_DMX_ERR_SYS,                     /**< System error*/
	AM_DMX_ERR_NO_DATA,                 /**< No data received*/
	AM_DMX_ERR_END
};

/**\brief Input source of the demux*/
typedef enum
{
	AM_DMX_SRC_TS0, 				   /**< TS input port 0*/
	AM_DMX_SRC_TS1, 				   /**< TS input port 1*/
	AM_DMX_SRC_TS2, 				   /**< TS input port 2*/
	AM_DMX_SRC_TS3, 				   /**< TS input port 3*/
	AM_DMX_SRC_HIU, 					/**< HIU input (memory)*/
	AM_DMX_SRC_HIU1
} AM_DMX_Source_t;

/**\brief 解复用设备*/
//typedef struct AM_DMX_Device AM_DMX_Device_t;

/**\brief 过滤器*/
//typedef struct AM_DMX_Filter AM_DMX_Filter_t;

/**\brief 过滤器位屏蔽*/
typedef uint32_t AM_DMX_FilterMask_t;

#define AM_DMX_FILTER_MASK_ISEMPTY(m)    (!(*(m)))
#define AM_DMX_FILTER_MASK_CLEAR(m)      (*(m)=0)
#define AM_DMX_FILTER_MASK_ISSET(m,i)    (*(m)&(1<<(i)))
#define AM_DMX_FILTER_MASK_SET(m,i)      (*(m)|=(1<<(i)))

class AmHwMultiDemuxWrapper;

typedef void (*AM_DMX_DataCb) (AmHwMultiDemuxWrapper* mDemuxWrapper, int fhandle, const uint8_t *data, int len, void *user_data);

struct AM_DMX_Filter {
	void	  *drv_data; /**< 驱动私有数据*/
	bool  used;  /**< 此Filter是否已经分配*/
	bool  enable;	 /**< 此Filter设备是否使能*/
	int 	   id;		 /**< Filter ID*/
	AM_DMX_DataCb		cb; 	   /**< 解复用数据回调函数*/
	void			   *user_data; /**< 数据回调函数用户参数*/
	bool to_be_stopped;
};
class AmLinuxDvd;

class AM_DMX_Device : public RefBase{

public:
	AM_DMX_Device(AmHwMultiDemuxWrapper* DemuxWrapper);
	~AM_DMX_Device();
	AM_ErrorCode_t dmx_get_used_filter(int filter_id, AM_DMX_Filter **pf);
	static void* dmx_data_thread(void *arg);
	AM_ErrorCode_t dmx_wait_cb(void);
	AM_ErrorCode_t dmx_stop_filter(AM_DMX_Filter *filter);
	int dmx_free_filter(AM_DMX_Filter *filter);
	AM_ErrorCode_t AM_DMX_Open(int dev_no_t);
	AM_ErrorCode_t AM_DMX_Close(void);
	AM_ErrorCode_t AM_DMX_AllocateFilter(int *fhandle);
	AM_ErrorCode_t AM_DMX_SetSecFilter(int fhandle, const struct dmx_sct_filter_params *params);
	AM_ErrorCode_t AM_DMX_SetPesFilter(int fhandle, const struct dmx_pes_filter_params *params);
	AM_ErrorCode_t AM_DMX_GetSTC(int fhandle);
	AM_ErrorCode_t AM_DMX_FreeFilter(int fhandle);
	AM_ErrorCode_t AM_DMX_StartFilter(int fhandle);
	AM_ErrorCode_t AM_DMX_StopFilter(int fhandle);
	AM_ErrorCode_t AM_DMX_SetBufferSize(int fhandle, int size);
	AM_ErrorCode_t AM_DMX_GetCallback(int fhandle, AM_DMX_DataCb *cb, void **data);
	AM_ErrorCode_t AM_DMX_SetCallback(int fhandle, AM_DMX_DataCb cb, void *data);
	//AM_ErrorCode_t AM_DMX_SetSource(AM_DMX_Source_t src);
	AM_ErrorCode_t AM_DMX_Sync();
	//AM_ErrorCode_t AM_DMX_GetScrambleStatus(AM_Bool_t dev_status[2]);
    static AM_ErrorCode_t AM_DMX_handlePESpacket(AM_DMX_Device *dev, AM_DMX_Filter *filter, unsigned char * outbuf, int* outlen, void *userdata);

	AM_ErrorCode_t AM_DMX_WriteTs(uint8_t* data,int32_t size,uint64_t timeout);
	int dev_no;      /**< 设备号*/
	sp<AmLinuxDvd> drv;  /**< 设备驱动*/
	void *drv_data;/**< 驱动私有数据*/
	AM_DMX_Filter filters[DMX_FILTER_COUNT];   /**< 设备中的Filter*/
	AmHwMultiDemuxWrapper* mDemuxWrapper;
private:
	int                 open_count; /**< 设备已经打开次数*/
	bool           enable_thread; /**< 数据线程已经运行*/
	int                 flags;   /**< 线程运行状态控制标志*/
	pthread_t           thread;  /**< 数据检测线程*/
	pthread_mutex_t     lock;    /**< 设备保护互斥体*/
	pthread_cond_t      cond;    /**< 条件变量*/
	//AM_DMX_Source_t     src;     /**< TS输入源*/
};


/*24~31 one byte for audio type, dmx_audio_format_t*/
#define DMX_AUDIO_FORMAT_BIT 24
#define CONFIG_AMLOGIC_DVB_COMPAT

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
#define ACODEC_FMT_MPEG1  26 // AFORMAT_MPEG-->mp3,AFORMAT_MPEG1-->mp1,AFROMAT_MPEG2-->mp2
#define ACODEC_FMT_MPEG2  27
#define ACODEC_FMT_WMAVOI 28
#define DMX_KERNEL_CLIENT   0x8000

#ifdef CONFIG_AMLOGIC_DVB_COMPAT
#define DMX_USE_SWFILTER    0x100
#endif

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


#ifdef CONFIG_AMLOGIC_DVB_COMPAT
#define DMX_MEM_SEC_LEVEL1   (1 << 10)
#define DMX_MEM_SEC_LEVEL2   (2 << 10)
#define DMX_MEM_SEC_LEVEL3   (3 << 10)
#define DMX_MEM_SEC_LEVEL4   (4 << 10)
#define DMX_MEM_SEC_LEVEL5   (5 << 10)
#define DMX_MEM_SEC_LEVEL6   (6 << 10)
#define DMX_MEM_SEC_LEVEL7   (7 << 10)

#define DMX_ES_OUTPUT        (1 << 16)
#define DMX_OUTPUT_RAW_MODE  (1 << 17)
#endif


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



#define DMX_GET_CAPS             _IOR('o', 48, dmx_caps_t)
#define DMX_SET_SOURCE           _IOW('o', 49, dmx_source_t)

#ifdef CONFIG_AMLOGIC_DVB_COMPAT
#define DMX_SET_INPUT           _IO('o', 80)
#endif
}

#endif
