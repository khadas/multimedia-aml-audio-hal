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



#ifndef  _AUDIO_AVSYNC_TABLE_H_
#define _AUDIO_AVSYNC_TABLE_H_

/* we use this table to tune the AV sync case for A/V playback including Tunnel
 * and None-Tunnel mode with the PCM/RAW source to audio HAL
 * if the value is positive (+), it can delay video as to give  smaller reported audio position (APTS)
 * if the value is negetive(-), it can advance the video as to give larger reported audio position(APTS)
 */

/*below MS12 tunning default value for different source and tunnel/none-tunnel*/
#define  AVSYNC_MS12_NONTUNNEL_PCM_LATENCY               (60)
#define  AVSYNC_MS12_NONTUNNEL_RAW_LATENCY               (40)
#define  AVSYNC_MS12_NONTUNNEL_ATMOS_LATENCY             (32)
#define  AVSYNC_MS12_TUNNEL_PCM_LATENCY                  (80)
#define  AVSYNC_MS12_TUNNEL_RAW_LATENCY                  (30)
#define  AVSYNC_MS12_TUNNEL_ATMOS_LATENCY                (32)

/*
below is the default value for different output format  for  HDMITX/ARC sink
*/
#define  AVSYNC_HDMI_PCM_LATENCY           (-20)
#define  AVSYNC_HDMI_DD_LATENCY            (-80)
#define  AVSYNC_HDMI_DDP_LATENCY           (-190)
#define  AVSYNC_HDMI_AC4_LATENCY           (-100)
#define  AVSYNC_HDMI_MAT_LATENCY           (-100)
/*below is the defalue value for atmos output of ARC sink  */
#define  AUDIO_ATMOS_HDMI_AUTO_LATENCY              -60

#define VENDOR_AUDIO_MS12_PASSTHROUGH_LATENCY		80

#define AVSYNC_SPEAKER_PCM_LATENCY (-0)
#define AVSYNC_SPEAKER_DD_LATENCY  (-30)
#define AVSYNC_SPEAKER_DDP_LATENCY (-30)
#define AVSYNC_SPEAKER_AC4_LATENCY (-30)
#define AVSYNC_SPEAKER_MAT_LATENCY (-10)



#define AUDIO_ATMOS_HDMI_LANTCY_PROPERTY "vendor.audio.hal.atmos.latency"
#define AUDIO_ATMOS_HDMI_PASSTHROUGH_PROPERTY "vendor.audio.hal.amtms.passthrough.latency"


/* ms12 version */
#define  AVSYNC_MS12_NONTUNNEL_PCM_LATENCY_PROPERTY      "vendor.audio.hal.ms12.nontunnel.pcm"
#define  AVSYNC_MS12_NONTUNNEL_RAW_LATENCY_PROPERTY      "vendor.audio.hal.ms12.nontunnel.raw"
#define  AVSYNC_MS12_NONTUNNEL_ATMOS_LATENCY_PROPERTY    "vendor.audio.hal.ms12.nontunnel.atmos"
#define  AVSYNC_MS12_TUNNEL_PCM_LATENCY_PROPERTY         "vendor.audio.hal.ms12.tunnel.pcm"
#define  AVSYNC_MS12_TUNNEL_RAW_LATENCY_PROPERTY         "vendor.audio.hal.ms12.tunnel.raw"
#define  AVSYNC_MS12_TUNNEL_ATMOS_LATENCY_PROPERTY       "vendor.audio.hal.ms12.tunnel.atmos"

/*HDMITX/ARC sink format  tunning */
#define  AVSYNC_HDMI_PCM_LATENCY_PROPERTY         "vendor.audio.hal.hdmi.pcm"
#define  AVSYNC_HDMI_DD_LATENCY_PROPERTY          "vendor.audio.hal.hdmi.dd"
#define  AVSYNC_HDMI_DDP_LATENCY_PROPERTY         "vendor.audio.hal.hdmi.ddp"
#define AVSYNC_HDMI_AC4_LATENCY_PROPERTY            "vendor.audio.hal.hdmi.ac4"
#define AVSYNC_HDMI_MAT_LATENCY_PROPERTY            "vendor.audio.hal.hdmi.mat"

/*HDMITX/ARC sink format  tunning */
#define AVSYNC_SPEAKER_PCM_LATENCY_PROPERTY "vendor.audio.hal.speaker.pcm"
#define AVSYNC_SPEAKER_DD_LATENCY_PROPERTY "vendor.audio.hal.speaker.dd"
#define AVSYNC_SPEAKER_DDP_LATENCY_PROPERTY "vendor.audio.hal.speaker.ddp"
#define AVSYNC_SPEAKER_AC4_LATENCY_PROPERTY "vendor.audio.hal.speaker.ac4"
#define AVSYNC_SPEAKER_MAT_LATENCY_PROPERTY "vendor.audio.hal.speaker.mat"
/* DDP version */
#define  AVSYNC_DDP_NONTUNNEL_PCM_LATENCY               (0)
#define  AVSYNC_DDP_NONTUNNEL_RAW_LATENCY               (0)
#define  AVSYNC_DDP_TUNNEL_PCM_LATENCY                  (0)
#define  AVSYNC_DDP_TUNNEL_RAW_LATENCY                  (0)


#define  AVSYNC_DDP_NONTUNNEL_PCM_LATENCY_PROPERTY      "vendor.audio.hal.ddp.nontunnel.pcm"
#define  AVSYNC_DDP_NONTUNNEL_RAW_LATENCY_PROPERTY      "vendor.audio.hal.ddp.nontunnel.raw"
#define  AVSYNC_DDP_TUNNEL_PCM_LATENCY_PROPERTY         "vendor.audio.hal.ddp.tunnel.pcm"
#define  AVSYNC_DDP_TUNNEL_RAW_LATENCY_PROPERTY         "vendor.audio.hal.ddp.tunnel.raw"



#endif

