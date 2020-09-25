/*
* Copyright (c) 2014 Amlogic, Inc. All rights reserved.
* *
This source code is subject to the terms and conditions defined in the
* file 'LICENSE' which is part of this source code package.
* *
Description:
*/


#ifndef ADEC_ASSOC_AUDIO_H_
#define ADEC_ASSOC_AUDIO_H_

#define VALID_PID(_pid_) ((_pid_)>0 && (_pid_)<0x1fff)
/* currently we only support EAC3/AC3/MPEG/AAC/AAC_LATM */
#define VALID_AD_FMT(fmt)  ((fmt == ACODEC_FMT_EAC3) || (fmt == ACODEC_FMT_AC3) || (fmt == ACODEC_FMT_MPEG) || (fmt == ACODEC_FMT_AAC) || (fmt == ACODEC_FMT_AAC_LATM))

void dtv_assoc_set_main_frame_size(int main_frame_size);
void dtv_assoc_get_main_frame_size(int* main_frame_size);
void dtv_assoc_set_ad_frame_size(int ad_frame_size);
void dtv_assoc_get_ad_frame_size(int* ad_frame_size);
void dtv_assoc_audio_cache(int value);
int dtv_assoc_audio_start(unsigned int handle,int pid,int fmt);
void dtv_assoc_audio_stop(unsigned int handle);
//void dtv_assoc_audio_pause(unsigned int handle);
//void dtv_assoc_audio_resume(unsigned int handle,int pid);
int dtv_assoc_get_avail(void);
int dtv_assoc_read(unsigned char *data, int size);
int dtv_assoc_init(void);
int dtv_assoc_deinit(void);

#endif

