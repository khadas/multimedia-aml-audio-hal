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



#define LOG_TAG "audio_hw_utils"
//#define LOG_NDEBUG 0
#define __USE_GNU

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <utils/Timers.h>
#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>
#include <linux/ioctl.h>
#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include <sound/asound.h>
#include <tinyalsa/asoundlib.h>

#include "audio_hw_utils.h"
#include "audio_hw_ms12.h"
#include "audio_hwsync.h"
#include "audio_hw.h"
#include "amlAudioMixer.h"
#include <audio_utils/primitives.h>
#include "audio_a2dp_hw.h"
#include "aml_audio_avsync_table.h"
#include "dolby_lib_api.h"
#ifdef USE_DTV
// for dtv playback
#include "audio_hw_dtv.h"
#define ENABLE_DTV_PATCH
#endif


#ifdef LOG_NDEBUG_FUNCTION
#define LOGFUNC(...) ((void)0)
#else
#define LOGFUNC(...) (ALOGD(__VA_ARGS__))
#endif

#define DD_MUTE_FRAME_SIZE 1536
#define DDP_MUTE_FRAME_SIZE 6144

// add array of dd/ddp mute frame for mute function
static const unsigned int muted_frame_dd[DD_MUTE_FRAME_SIZE] = {
 0x4e1ff872, 0x8000001, 0xb49e0b77, 0x43e10840, 0x10806f0, 0x21010808,
 0x571f0104, 0xf9f33e7c, 0x9f3ee7cf, 0xfe757cfb, 0xf3e77cf9, 0x3e7ccf9f,
 0x9d5ff8ff, 0xf9f33e7c, 0x9f3ee7cf, 0x24432ff, 0x8920, 0xb5f30001,
 0x9b6de7cf, 0x6f1eb6db, 0x3c00, 0x9f3e0daf, 0x6db67cdb, 0xf1e1db78,
 0x8000, 0xd7cf0006, 0x6db69f3e, 0xbc78db6d, 0xf000, 0x7cf936be,
 0xb6dbf36d, 0xc7866de3,0x0, 0x5f3e001b, 0xb6db7cf9, 0xf1e36db6,
 0xc000, 0xf3e7daf9, 0xdb6dcdb6, 0x1e18b78f, 0x0, 0x7cf9006d,
 0xdb6df3e6, 0xc78fb6db, 0x30000, 0xcf9f6be7, 0x6db636db, 0x7860de3c,
 0x0, 0xf3e701b5, 0x6db6cf9b, 0x1e3cdb6f, 0xd0000, 0x3e7caf9f, 0xb6dbdb6d,
 0xe18078f1, 0x0, 0xcf9f06d7, 0xb6db3e6d, 0x78f06dbc, 0x360000, 0xf9f3be7c,
 0xdb6d6db6, 0x8000e3c7,0x0, 0xe2560000
};

static const unsigned int muted_frame_ddp[DDP_MUTE_FRAME_SIZE] = {
    0x4e1ff872,  0xbe40715,  0x5f10b77, 0xfffa3f67,   0x484900,    0x40000,    0x80000, 0x6186e100,
    0xff3a1861, 0xf9f3be7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7,
    0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c,
    0xfceae7df, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f,
    0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3,
    0xf3ab9f7f, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c,
    0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf,
    0xceaf7dff, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3,
    0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e,
    0x3abef7ff, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf,
    0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9,
    0xeb94dffc, 0x137ef8e0, 0xc3c7c280, 0x3bbb8f1e, 0xb800bbbb,          0,          0,          0,
             0,          0,   0x770000, 0x6db67777, 0xb7cfdb6d, 0xb5ad9ad6, 0x3e7c6b5f, 0xe7cff9f3,
    0x7cf99f3e, 0xcfb9f3e7, 0x27d33873, 0xe3c778f1, 0x77777777,          0,          0,          0,
             0,          0,          0, 0xeeed0eee, 0x6db6b6db, 0x5ad6f9f3, 0x6be7b5ad, 0x3e7ccf9f,
    0xe7cff9f3, 0x7cf99f3e, 0xf21bf0d9, 0x1e3c876f, 0xeeee78ee,     0xeee0,          0,          0,
             0,          0,          0, 0xdddd0001, 0xdb6dddb6, 0x3e6bb6df, 0xb5ad5ad6, 0xf3e77cf9,
    0x3e7ccf9f, 0xe7cff9f3, 0xe4e19f3e, 0x4de3cc9f, 0x1dddc78f, 0xdc00dddd,          0,          0,
             0,          0,          0,   0x3b0000, 0xb6dbbbbb, 0xdbe76db6, 0x5ad6cd6b, 0x9f3eb5af,
    0xf3e77cf9, 0x3e7ccf9f, 0xe7c3f9f3, 0x6e1d67c8, 0xf1e3bc78, 0xbbbbbbbb,     0x8000,          0,
             0,          0,          0,          0, 0x77760777, 0xb6dbdb6d, 0xad6b7cf9, 0xb5f35ad6,
    0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xffc0f804, 0x90008077, 0x614009bf, 0xc78f61e3, 0xdddd1ddd,
        0xdc00,          0,          0,          0,          0,          0, 0xbbbb003b, 0x6db6b6db,
    0xcd6bdbe7, 0xb5af5ad6, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9c39e7dc, 0xbc7893e9, 0xbbbbf1e3,
    0x8000bbbb,          0,          0,          0,          0,          0,  0x7770000, 0xdb6d7776,
    0x7cf9b6db, 0x5ad6ad6b, 0xe7cfb5f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf86c3e7c, 0xc3b7f90d, 0x3c778f1e,
    0x77707777,          0,          0,          0,          0,          0,          0, 0xeedbeeee,
    0xdb6f6db6, 0xad6b9f35, 0xbe7c5ad6, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xe64f7270, 0xe3c7a6f1,
    0xeeee8eee,     0xee00,          0,          0,          0,          0,          0, 0xdddd001d,
    0xb6dbdb6d, 0xe6b56df3, 0x5ad7ad6b, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xb3e4f3e1, 0xde3c370e,
    0xdddd78f1, 0xc000dddd,          0,          0,          0,          0,          0,  0x3bb0000,
    0x6db6bbbb, 0xbe7cdb6d, 0xad6bd6b5, 0xf3e75af9, 0x3e7ccf9f, 0xe7cff9f3, 0x7c029f3e, 0x403b7fe0,
    0xfa00c800,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,    0x20000, 0x50186fd8, 0xe3c778f1, 0x77777777,          0,          0,
             0,          0,          0,          0, 0xeeed0eee, 0x6db6b6db, 0x5ad6f9f3, 0x6be7b5ad,
    0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e,  0xe64f727, 0x1e3cfa6f, 0xeeee78ee,     0xeee0,          0,
             0,          0,          0,          0, 0xdddd0001, 0xdb6dddb6, 0x3e6bb6df, 0xb5ad5ad6,
    0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x1b3e9f3e, 0xede34370, 0x1dddc78f, 0xdc00dddd,          0,
             0,          0,          0,          0,   0x3b0000, 0xb6dbbbbb, 0xdbe76db6, 0x5ad6cd6b,
    0x9f3eb5af, 0xf3e77cf9, 0x3e7ccf9f, 0xe7dcf9f3, 0x93e99c39, 0xf1e3bc78, 0xbbbbbbbb,     0x8000,
             0,          0,          0,          0,          0, 0x77760777, 0xb6dbdb6d, 0xad6b7cf9,
    0xb5f35ad6, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xf90df86c, 0x8f1ec3b7, 0x77773c77,     0x7770,
             0,          0,          0,          0,          0, 0xeeee0000, 0x6db6eedb, 0x9f35db6f,
    0x5ad6ad6b, 0xf9f3be7c, 0x9f3ee7cf, 0xf3e77cf9,   0x9fcf9f,  0xef2f810, 0x37ec0001, 0x3c78280c,
    0xbbbbf1e3, 0x8000bbbb,          0,          0,          0,          0,          0,  0x7770000,
    0xdb6d7776, 0x7cf9b6db, 0x5ad6ad6b, 0xe7cfb5f3, 0x7cf99f3e, 0xcf9ff3e7, 0xfb933e7c, 0x7d378732,
    0x3c778f1e, 0x77707777,          0,          0,          0,          0,          0,          0,
    0xeedbeeee, 0xdb6f6db6, 0xad6b9f35, 0xbe7c5ad6, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0x21b80d9f,
    0xe3c776f1, 0xeeee8eee,     0xee00,          0,          0,          0,          0,          0,
    0xdddd001d, 0xb6dbdb6d, 0xe6b56df3, 0x5ad7ad6b, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0x4e1cf3ee,
    0xde3cc9f4, 0xdddd78f1, 0xc000dddd,          0,          0,          0,          0,          0,
    0x3bb0000, 0x6db6bbbb, 0xbe7cdb6d, 0xad6bd6b5, 0xf3e75af9, 0x3e7ccf9f, 0xe7cff9f3, 0x7c369f3e,
    0xe1db7c86, 0x1e3bc78f, 0xbbb8bbbb,          0,          0,          0,          0,          0,
             0, 0x776d7777, 0x6db7b6db, 0xd6b5cf9a, 0x5f3ead6b, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3,
    0xfc08804f,      0x779, 0x14069bf6, 0x78f11e3c, 0xdddddddd,     0xc000,          0,          0,
             0,          0,          0, 0xbbbb03bb, 0xdb6d6db6, 0xd6b5be7c, 0x5af9ad6b, 0xcf9ff3e7,
    0xf9f33e7c, 0x9f3ee7cf, 0xc3997dc9, 0xc78f3e9b, 0xbbbb1e3b,     0xbbb8,          0,          0,
             0,          0,          0, 0x77770000, 0xb6db776d, 0xcf9a6db7, 0xad6bd6b5, 0x7cf95f3e,
    0xcf9ff3e7, 0xf9f33e7c, 0x86cfe7cf, 0x3b7890dc, 0xc777f1e3, 0x77007777,          0,          0,
             0,          0,          0,    0xe0000, 0xedb6eeee, 0xb6f9db6d, 0xd6b5f35a, 0xe7cfad6b,
    0x7cf99f3e, 0xcf9ff3e7, 0xf9f73e7c, 0x64fa270e, 0x3c786f1e, 0xeeeeeeee,     0xe000,          0,
             0,          0,          0,          0, 0xdddd01dd, 0x6db6b6db, 0x6b5adf3e, 0xad7cd6b5,
    0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0x3e433e1b, 0xe3c770ed, 0xdddd8f1d,     0xdddc,          0,
             0,          0,          0,          0, 0x3bbb0000, 0xdb6dbbb6, 0xe7cdb6db, 0xd6b56b5a,
    0x3e7caf9f, 0xe7cff9f3, 0x7cf99f3e, 0xc027f3e7,  0x3bcfe04, 0x4dfb8000,  0xf1e0a03, 0xeeee3c78,
    0xe000eeee,          0,          0,          0,          0,          0,  0x1dd0000, 0xb6dbdddd,
    0xdf3e6db6, 0xd6b56b5a, 0xf9f3ad7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3ee4cf9f, 0x9f4de1cc, 0x8f1de3c7,
    0xdddcdddd,          0,          0,          0,          0,          0,          0, 0xbbb63bbb,
    0xb6dbdb6d, 0x6b5ae7cd, 0xaf9fd6b5, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0xc86ec367, 0x78f11dbc,
    0xbbbbe3bb,     0xbb80,          0,          0,          0,          0,          0, 0x77770007,
    0x6db676db, 0xf9addb7c, 0xd6b56b5a, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0x93877cfb, 0x378f327d,
    0x77771e3c, 0x70007777,          0,          0,          0,          0,          0,   0xee0000,
    0xdb6deeee, 0x6f9fb6db, 0x6b5a35ad, 0x7cf9d6be, 0xcf9ff3e7, 0xf9f33e7c, 0x9f0de7cf, 0xb8769f21,
    0xc78ef1e3, 0xeeeeeeee,          0,          0,          0,          0,          0,          0,
    0xdddb1ddd, 0xdb6d6db6, 0xb5adf3e6, 0xd7cf6b5a, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0xff02e013,
         0x1de,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,    0x30000,  0x8a1e6a4,          0,          0,          0,          0,          0,

};

static const unsigned int muted_frame_atmos[DDP_MUTE_FRAME_SIZE] = {
    0x4e1ff872,  0xbe40715,  0x5f10b77, 0xfffa3f67, 0x41014900, 0x20000f01, 0x10000000, 0x23840000,
    0x18610186, 0xeaf987fc, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3,
    0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e,
    0xcf9ff3e7, 0xabe77ff3, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf,
    0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9,
    0x3e7dcf9f, 0xaf9fffce, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e,
    0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7,
    0xf9f73e7c, 0xbe7cff3a, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9,
    0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f,
    0xe7dff9f3, 0xf9f3fcea, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7,
    0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c,
    0x9f7fe7cf, 0x53e3f3ae,   0x7f87fe, 0x1e3c80ef, 0xeeee78ee,     0xeee0,          0,          0,
             0,          0,          0, 0xdddd0001, 0xdb6dddb6, 0x3e6bb6df, 0xb5ad5ad6, 0xf3e77cf9,
    0x3e7ccf9f, 0xe7cff9f3,  0x23f9f3e, 0x1de3e040, 0x1dddc78f, 0xdc00dddd,          0,          0,
             0,          0,          0,   0x3b0000, 0xb6dbbbbb, 0xdbe76db6, 0x5ad6cd6b, 0x9f3eb5af,
    0xf3e77cf9, 0x3e7ccf9f, 0xe7c0f9f3,    0x30400, 0xf1e3bc78, 0xbbbbbbbb,     0x8000,          0,
             0,          0,          0,          0, 0x77760777, 0xb6dbdb6d, 0xad6b7cf9, 0xb5f35ad6,
    0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0x8000f801, 0x8f1e4077, 0x77773c77,     0x7770,          0,
             0,          0,          0,          0, 0xeeee0000, 0x6db6eedb, 0x9f35db6f, 0x5ad6ad6b,
    0xf9f3be7c, 0x9f3ee7cf, 0xf3e77cf9,   0x30cf9f,  0xef10008, 0x8eeee3c7, 0xee00eeee,          0,
             0,          0,          0,          0,   0x1d0000, 0xdb6ddddd, 0x6df3b6db, 0xad6be6b5,
    0xcf9f5ad7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e07cf9,  0xb0657fb,  0x3ff1e40, 0xc077003f, 0x3c778f1e,
    0x77707777,          0,          0,          0,          0,          0,          0, 0xeedbeeee,
    0xdb6f6db6, 0xad6b9f35, 0xbe7c5ad6, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf020011f, 0xe3c70ef1,
    0xeeee8eee,     0xee00,          0,          0,          0,          0,          0, 0xdddd001d,
    0xb6dbdb6d, 0xe6b56df3, 0x5ad7ad6b, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e,  0x200f3e0, 0xde3c0001,
    0xdddd78f1, 0xc000dddd,          0,          0,          0,          0,          0,  0x3bb0000,
    0x6db6bbbb, 0xbe7cdb6d, 0xad6bd6b5, 0xf3e75af9, 0x3e7ccf9f, 0xe7cff9f3, 0x7c009f3e, 0x203bc000,
    0x1e3bc78f, 0xbbb8bbbb,          0,          0,          0,          0,          0,          0,
    0x776d7777, 0x6db7b6db, 0xd6b5cf9a, 0x5f3ead6b, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3,    0x48018,
    0xf1e30778, 0x7777c777,     0x7700,          0,          0,          0,          0,          0,
    0xeeee000e, 0xdb6dedb6, 0xf35ab6f9, 0xad6bd6b5, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0x2bfdf9f0,
     0xf208583, 0x583803e8,  0x86900ce,     0x81c0, 0x5e000c04, 0x2000b83f, 0x8443481f, 0xa27fe000,
    0x40ffbe80, 0x81df3f00,  0x3fc0201,  0xa100402, 0x41501f80,  0x20a81f4, 0x20108400, 0x1f900074,
     0x384008f, 0xa9a90161, 0xa9a9a9a9, 0xa9a9a9a9, 0xa9a9a9a9,     0xa800, 0x60180001,     0x5954,
       0x90000, 0x88dad018,    0xc0000,     0x1600,  0x3050000,     0x8000,          0,          0,
             0,          0,          0, 0x4492000c,     0x8000,          0,          0,          0,
             0,          0,          0, 0xb2d00030,          0,          0,          0,          0,
             0,          0,     0x182c,          0, 0x61750002, 0x21802d12, 0xcdcdcdcd, 0xcdcdcdcd,
    0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd,
    0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd,
    0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd,
    0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd,
    0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd,
    0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd,
    0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd,
    0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd,
    0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xffc0cd00, 0x1de30ff0, 0x1dddc78f, 0xdc00dddd,          0,
             0,          0,          0,          0,   0x3b0000, 0xb6dbbbbb, 0xdbe76db6, 0x5ad6cd6b,
    0x9f3eb5af, 0xf3e77cf9, 0x3e7ccf9f, 0xe7c0f9f3,  0x80347fc, 0xf1e3bc78, 0xbbbbbbbb,     0x8000,
             0,          0,          0,          0,          0, 0x77760777, 0xb6dbdb6d, 0xad6b7cf9,
    0xb5f35ad6, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0x8000f800, 0x8f1e0077, 0x77773c77,     0x7770,
             0,          0,          0,          0,          0, 0xeeee0000, 0x6db6eedb, 0x9f35db6f,
    0x5ad6ad6b, 0xf9f3be7c, 0x9f3ee7cf, 0xf3e77cf9,   0x30cf9f,  0xef10008, 0x8eeee3c7, 0xee00eeee,
             0,          0,          0,          0,          0,   0x1d0000, 0xdb6ddddd, 0x6df3b6db,
    0xad6be6b5, 0xcf9f5ad7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e07cf9,  0x1010600, 0x78f1de3c, 0xdddddddd,
        0xc000,          0,          0,          0,          0,          0, 0xbbbb03bb, 0xdb6d6db6,
    0xd6b5be7c, 0x5af9ad6b, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xff617c0a, 0xc80060c3,  0x7f87fe0,
    0xe3c70ef1, 0xeeee8eee,     0xee00,          0,          0,          0,          0,          0,
    0xdddd001d, 0xb6dbdb6d, 0xe6b56df3, 0x5ad7ad6b, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0x23fef3e0,
    0xde3c0401, 0xdddd78f1, 0xc000dddd,          0,          0,          0,          0,          0,
     0x3bb0000, 0x6db6bbbb, 0xbe7cdb6d, 0xad6bd6b5, 0xf3e75af9, 0x3e7ccf9f, 0xe7cff9f3, 0x7c009f3e,
      0x3b4000, 0x1e3bc78f, 0xbbb8bbbb,          0,          0,          0,          0,          0,
             0, 0x776d7777, 0x6db7b6db, 0xd6b5cf9a, 0x5f3ead6b, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3,
       0x48018, 0xf1e30778, 0x7777c777,     0x7700,          0,          0,          0,          0,
             0, 0xeeee000e, 0xdb6dedb6, 0xf35ab6f9, 0xad6bd6b5, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f,
     0x300f9f0, 0xef1e0080, 0xeeee3c78, 0xe000eeee,          0,          0,          0,          0,
              0,  0x1dd0000, 0xb6dbdddd, 0xdf3e6db6, 0xd6b56b5a, 0xf9f3ad7c, 0x9f3ee7cf, 0xf3e77cf9,
    0x3e05cf9f, 0xb0617fb0, 0x3ff0e400,  0x77803fc, 0xc777f1e3, 0x77007777,          0,          0,
             0,          0,          0,    0xe0000, 0xedb6eeee, 0xb6f9db6d, 0xd6b5f35a, 0xe7cfad6b,
    0x7cf99f3e, 0xcf9ff3e7, 0xf9f03e7c,  0x20011ff, 0x3c78ef1e, 0xeeeeeeee,     0xe000,          0,
             0,          0,          0,          0, 0xdddd01dd, 0x6db6b6db, 0x6b5adf3e, 0xad7cd6b5,
    0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0x20003e00, 0xe3c7001d, 0xdddd8f1d,     0xdddc,          0,
             0,          0,          0,          0, 0x3bbb0000, 0xdb6dbbb6, 0xe7cdb6db, 0xd6b56b5a,
    0x3e7caf9f, 0xe7cff9f3, 0x7cf99f3e, 0xc00cf3e7,  0x3bc0002, 0xe3bb78f1, 0xbb80bbbb,          0,
             0,          0,          0,          0,    0x70000, 0x76db7777, 0xdb7c6db6, 0x6b5af9ad,
    0xf3e7d6b5, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf89f3e,   0x400180, 0x1e3c778f, 0x77777777,     0x7000,
             0,          0,          0,          0,          0, 0xeeee00ee, 0xb6dbdb6d, 0x35ad6f9f,
    0xd6be6b5a, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0xbfd89f02, 0xf2005830,  0x1fe1ff8, 0x78f103bc,
    0xbbbbe3bb,     0xbb80,          0,          0,          0,          0,          0, 0x77770007,
    0x6db676db, 0xf9addb7c, 0xd6b56b5a, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf,  0x8ff7cf8, 0x778f8100,
    0x77771e3c, 0x70007777,          0,          0,          0,          0,          0,   0xee0000,
    0xdb6deeee, 0x6f9fb6db, 0x6b5a35ad, 0x7cf9d6be, 0xcf9ff3e7, 0xf9f33e7c, 0x9f00e7cf,    0xe1000,
    0xc78ef1e3, 0xeeeeeeee,          0,          0,          0,          0,          0,          0,
    0xdddb1ddd, 0xdb6d6db6, 0xb5adf3e6, 0xd7cf6b5a, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c,    0x1e006,
    0x3c7801de, 0xddddf1dd,     0xddc0,          0,          0,          0,          0,          0,
    0xbbbb0003, 0xb6dbbb6d, 0x7cd66dbe, 0x6b5ab5ad, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7,   0xc03e7c,
    0x3bc70020, 0x3bbb8f1e, 0xb800bbbb,          0,          0,          0,          0,          0,
      0x770000, 0x6db67777, 0xb7cfdb6d, 0xb5ad9ad6, 0x3e7c6b5f, 0xe7cff9f3, 0x7cf99f3e, 0xcf81f3e7,
    0x2c185fec,     0x7800,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,
             0,    0x30000, 0x19b76198,          0,          0,          0,          0,          0,
};

//DRC Mode
#define DDPI_UDC_COMP_LINE 2
#define DRC_MODE_BIT  0
#define DRC_HIGH_CUT_BIT 3
#define DRC_LOW_BST_BIT 16
static const char *str_compmode[] = {"custom mode, analog dialnorm","custom mode, digital dialnorm",
                            "line out mode","RF remod mode"};

int64_t aml_gettime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((int64_t)(tv.tv_sec) * 1000000 + (int64_t)(tv.tv_usec));
}
int get_sysfs_uint(const char *path, uint *value)
{
    int fd, nread;
    char valstr[64];
    uint val = 0;
    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        memset(valstr, 0, 64);
        nread = read(fd, valstr, 64 - 1);
        if (nread > 0) {
            valstr[nread] = '\0';
        }
        close(fd);
    } else {
        ALOGE("unable to open file %s\n", path);
        return -1;
    }
    if (sscanf(valstr, "0x%x", &val) < 1) {
        ALOGE("unable to get pts from: %s", valstr);
        return -1;
    }
    *value = val;
    return 0;
}

int sysfs_set_sysfs_str(const char *path, const char *val)
{
    int fd;
    int bytes;
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0) {
        bytes = write(fd, val, strlen(val));
        close(fd);
        return 0;
    } else {
        ALOGE("unable to open file %s,err: %s", path, strerror(errno));
    }
    return -1;
}

int get_sysfs_int(const char *path)
{
    int val = 0, nread;
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        char bcmd[16];
        nread = read(fd, bcmd, sizeof(bcmd));
        if (nread > 0) {
            val = strtol(bcmd, NULL, 10);
        }
        close(fd);
    } else {
        ALOGD("[%s]open %s node failed! return 0\n", path, __FUNCTION__);
    }
    return val;
}
int mystrstr(char *mystr, char *substr)
{
    int i = 0;
    int j = 0;
    int score = 0;
    int substrlen = strlen(substr);
    int ok = 0;
    for (i = 0; i < 1024 - substrlen; i++) {
        for (j = 0; j < substrlen; j++) {
            score += (substr[j] == mystr[i + j]) ? 1 : 0;
        }
        if (score == substrlen) {
            ok = 1;
            break;
        }
        score = 0;
    }
    return ok;
}
void set_codec_type(int type)
{
    char buf[16];
    int fd = open("/sys/class/audiodsp/digital_codec", O_WRONLY);

    if (fd >= 0) {
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "%d", type);

        write(fd, buf, sizeof(buf));
        close(fd);
    }
}
unsigned char codec_type_is_raw_data(int type)
{
    switch (type) {
    case TYPE_AC3:
    case TYPE_EAC3:
    case TYPE_TRUE_HD:
    case TYPE_DTS:
    case TYPE_DTS_HD:
    case TYPE_DTS_HD_MA:
    case TYPE_AC4:
        return 1;
    default:
        return 0;
    }
}

int get_codec_type(int format)
{
    switch (format) {
    case AUDIO_FORMAT_AC3:
        return TYPE_AC3;
    case AUDIO_FORMAT_E_AC3:
        return TYPE_EAC3;
    case AUDIO_FORMAT_DTS:
        return TYPE_DTS;
    case AUDIO_FORMAT_DTS_HD:
        return TYPE_DTS_HD_MA;
    case AUDIO_FORMAT_DOLBY_TRUEHD:
    case AUDIO_FORMAT_MAT:
        return TYPE_TRUE_HD;
    case AUDIO_FORMAT_AC4:
        return TYPE_AC4;
    case AUDIO_FORMAT_PCM:
    case AUDIO_FORMAT_PCM_16_BIT:
    case AUDIO_FORMAT_PCM_32_BIT:
        return TYPE_PCM;
    default:
        return TYPE_PCM;
    }
}
int getprop_bool(const char *path)
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;

    ret = property_get(path, buf, NULL);
    if (ret > 0) {
        if (strcasecmp(buf, "true") == 0 || strcmp(buf, "1") == 0) {
            return 1;
        }
    }
    return 0;
}

int is_txlx_chip()
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;

    ret = property_get("ro.board.platform", buf, NULL);
    if (ret > 0) {
        if (strcasecmp(buf, "txlx") == 0) {
            return true;
        }
    }
    return false;
}

int is_txl_chip()
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;

    ret = property_get("ro.board.platform", buf, NULL);
    if (ret > 0) {
        if (strcasecmp(buf, "txl") == 0) {
            return true;
        }
    }
    return false;
}

int is_sc2_chip()
{
#ifdef DVB_AUDIO_SC2
    return true;
#endif
    return false;
}

/*
convert audio formats to supported audio format
8 ch goes to 32 bit
2 ch can be 16 bit or 32 bit
@return input buffer used by alsa drivers to do the data write
*/
void *convert_audio_sample_for_output(int input_frames, int input_format, int input_ch, void *input_buf, int *out_size)
{
    float lvol = 1.0;
    int *out_buf = NULL;
    short *p16 = (short*)input_buf;
    int *p32 = (int*)input_buf;
    int max_ch =  input_ch;
    int i;
    //ALOGV("intput frame %d,input ch %d,buf ptr %p,vol %f\n", input_frames, input_ch, input_buf, lvol);
    ALOG_ASSERT(input_buf);
    if (input_ch > 2) {
        max_ch = 8;
    }
    //our HW need round the frames to 8 channels
    out_buf = malloc(sizeof(int) * max_ch * input_frames);
    if (out_buf == NULL) {
        ALOGE("malloc buffer failed\n");
        return NULL;
    }
    switch (input_format) {
    case AUDIO_FORMAT_PCM_16_BIT:
        break;
    case AUDIO_FORMAT_PCM_32_BIT:
        break;
    case AUDIO_FORMAT_PCM_8_24_BIT:
        for (i = 0; i < input_frames * input_ch; i++) {
            p32[i] = p32[i] << 8;
        }
        break;
    case AUDIO_FORMAT_PCM_FLOAT:
        memcpy_to_i16_from_float((short*)out_buf, input_buf, input_frames * input_ch);
        memcpy(input_buf, out_buf, sizeof(short)*input_frames * input_ch);
        break;
    }
    //current all the data are in the input buffer
    if (input_ch == 8) {
        short *p16_temp;
        int i, NumSamps;
        int *p32_temp = out_buf;
        float m_vol = lvol;
        NumSamps = input_frames * input_ch;
        //here to swap the channnl data here
        //actual now:L,missing,R,RS,RRS,,LS,LRS,missing
        //expect L,C,R,RS,RRS,LRS,LS,LFE (LFE comes from to center)
        //actual  audio data layout  L,R,C,none/LFE,LRS,RRS,LS,RS
        if (input_format == AUDIO_FORMAT_PCM_16_BIT) {
            p16_temp = (short*)out_buf;
            for (i = 0; i < NumSamps; i = i + 8) {
                p16_temp[0 + i]/*L*/ = m_vol * p16[0 + i];
                p16_temp[1 + i]/*R*/ = m_vol * p16[1 + i];
                p16_temp[2 + i] /*LFE*/ = m_vol * p16[3 + i];
                p16_temp[3 + i] /*C*/ = m_vol * p16[2 + i];
                p16_temp[4 + i] /*LS*/ = m_vol * p16[6 + i];
                p16_temp[5 + i] /*RS*/ = m_vol * p16[7 + i];
                p16_temp[6 + i] /*LRS*/ = m_vol * p16[4 + i];
                p16_temp[7 + i]/*RRS*/ = m_vol * p16[5 + i];
            }
            memcpy(p16, p16_temp, NumSamps * sizeof(short));
            for (i = 0; i < NumSamps; i++) { //suppose 16bit/8ch PCM
                p32_temp[i] = p16[i] << 16;
            }
        } else {
            p32_temp = out_buf;
            for (i = 0; i < NumSamps; i = i + 8) {
                p32_temp[0 + i]/*L*/ = m_vol * p32[0 + i];
                p32_temp[1 + i]/*R*/ = m_vol * p32[1 + i];
                p32_temp[2 + i] /*LFE*/ = m_vol * p32[3 + i];
                p32_temp[3 + i] /*C*/ = m_vol * p32[2 + i];
                p32_temp[4 + i] /*LS*/ = m_vol * p32[6 + i];
                p32_temp[5 + i] /*RS*/ = m_vol * p32[7 + i];
                p32_temp[6 + i] /*LRS*/ = m_vol * p32[4 + i];
                p32_temp[7 + i]/*RRS*/ = m_vol * p32[5 + i];
            }

        }
        *out_size = NumSamps * sizeof(int);

    } else if (input_ch == 6) {
        int j, NumSamps, real_samples;
        short *p16_temp;
        int *p32_temp = out_buf;
        float m_vol = lvol;
        NumSamps = input_frames * input_ch;
        real_samples = NumSamps;
        NumSamps = real_samples * 8 / 6;
        //ALOGI("6ch to 8 ch real %d, to %d\n",real_samples,NumSamps);
        if (input_format == AUDIO_FORMAT_PCM_16_BIT) {
            p16_temp = (short*)out_buf;
            for (i = 0; i < real_samples; i = i + 6) {
                p16_temp[0 + i]/*L*/ = m_vol * p16[0 + i];
                p16_temp[1 + i]/*R*/ = m_vol * p16[1 + i];
                p16_temp[2 + i] /*LFE*/ = m_vol * p16[3 + i];
                p16_temp[3 + i] /*C*/ = m_vol * p16[2 + i];
                p16_temp[4 + i] /*LS*/ = m_vol * p16[4 + i];
                p16_temp[5 + i] /*RS*/ = m_vol * p16[5 + i];
            }
            memcpy(p16, p16_temp, real_samples * sizeof(short));
            memset(p32_temp, 0, NumSamps * sizeof(int));
            for (i = 0, j = 0; j < NumSamps; i = i + 6, j = j + 8) { //suppose 16bit/8ch PCM
                p32_temp[j + 0] = p16[i] << 16;
                p32_temp[j + 1] = p16[i + 1] << 16;
                p32_temp[j + 2] = p16[i + 2] << 16;
                p32_temp[j + 3] = p16[i + 3] << 16;
                p32_temp[j + 4] = p16[i + 4] << 16;
                p32_temp[j + 5] = p16[i + 5] << 16;
            }
        } else {
            p32_temp = out_buf;
            memset(p32_temp, 0, NumSamps * sizeof(int));
            for (i = 0, j = 0; j < NumSamps; i = i + 6, j = j + 8) { //suppose 16bit/8ch PCM
                p32_temp[j + 0] = m_vol * p32[i + 0];
                p32_temp[j + 1] = m_vol * p32[i + 1] ;
                p32_temp[j + 2] = m_vol * p32[i + 2] ;
                p32_temp[j + 3] = m_vol * p32[i + 3] ;
                p32_temp[j + 4] = m_vol * p32[i + 4] ;
                p32_temp[j + 5] = m_vol * p32[i + 5] ;
            }
        }
        *out_size = NumSamps * sizeof(int);
    } else {
        //2ch with 24 bit/32/float audio
        int *p32_temp = out_buf;
        short *p16_temp = (short*)out_buf;
        for (i = 0; i < input_frames; i++) {
            p16_temp[2 * i + 0] =  lvol * p16[2 * i + 0];
            p16_temp[2 * i + 1] =  lvol * p16[2 * i + 1];
        }
        *out_size = sizeof(short) * input_frames * input_ch;
    }
    return out_buf;

}

int aml_audio_start_trigger(void *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    char tempbuf[128];
    ALOGI("reset alsa to set the audio start\n");
    pcm_stop(aml_out->pcm);
    sprintf(tempbuf, "AUDIO_START:0x%x", adev->first_apts);
    ALOGI("audio start set tsync -> %s", tempbuf);
    sysfs_set_sysfs_str(TSYNC_ENABLE, "1"); // enable avsync
    sysfs_set_sysfs_str(TSYNC_MODE, "1"); // enable avsync
    if (sysfs_set_sysfs_str(TSYNC_EVENT, tempbuf) == -1) {
        ALOGE("set AUDIO_START failed \n");
        return -1;
    }
    return 0;
}

int aml_audio_get_debug_flag()
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int debug_flag = 0;
    ret = property_get("media.audio.hal.debug", buf, NULL);
    if (ret > 0) {
        debug_flag = atoi(buf);
    }
    return debug_flag;
}

int aml_audio_debug_set_optical_format()
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;

    ret = property_get("media.audio.hal.optical", buf, NULL);
    if (ret > 0) {
        if (strcasecmp(buf, "pcm") == 0 || strcmp(buf, "0") == 0) {
            return TYPE_PCM;
        }
        if (strcasecmp(buf, "dd") == 0 || strcmp(buf, "1") == 0) {
            return TYPE_AC3;
        }
        if (strcasecmp(buf, "ddp") == 0 || strcmp(buf, "2") == 0) {
            return TYPE_EAC3;
        }
    }
    return -1;
}

int aml_audio_dump_audio_bitstreams(const char *path, const void *buf, size_t bytes)
{
    if (!path) {
        return 0;
    }

    FILE *fp = fopen(path, "a+");
    if (fp) {
        int flen = fwrite((char *)buf, 1, bytes, fp);
        fclose(fp);
    }

    return 0;
}

int aml_audio_get_ms12_latency_offset(int b_raw)
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;

    if (b_raw == 0) {
        /*for non tunnel ddp2h/heaac case:netlfix AL1 case */
        prop_name = AVSYNC_MS12_NONTUNNEL_PCM_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NONTUNNEL_PCM_LATENCY;
    }else {
        /*for non tunnel dolby ddp5.1 case:netlfix AL1 case*/
        prop_name = AVSYNC_MS12_NONTUNNEL_RAW_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NONTUNNEL_RAW_LATENCY;
    }

    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        latency_ms = atoi(buf);
    }
    /*because the caller use add fucntion, instead of minus, so we return the minus value*/
    return -latency_ms;
}
int aml_audio_get_ms12_tunnel_latency_offset(int b_raw)
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;
    /*tunnle mode case*/
    latency_ms = 0;

    if (b_raw == 0) {
        /*for non tunnel ddp2h/heaac case:netlfix AL1 case */
        prop_name = AVSYNC_MS12_TUNNEL_PCM_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_TUNNEL_PCM_LATENCY;
    } else {
        /*for non tunnel dolby ddp5.1 case:netlfix AL1 case*/
        prop_name = AVSYNC_MS12_TUNNEL_RAW_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_TUNNEL_RAW_LATENCY;
    }

    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        latency_ms = atoi(buf);
    }
    return latency_ms;
}

int aml_audio_get_ms12_atmos_latency_offset(int tunnel)
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;
    if (tunnel) {
        /*tunnel atmos case*/
        prop_name = AVSYNC_MS12_TUNNEL_ATMOS_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_TUNNEL_ATMOS_LATENCY;
    }else {
        /*non tunnel atmos case*/
        prop_name = AVSYNC_MS12_NONTUNNEL_ATMOS_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NONTUNNEL_ATMOS_LATENCY;
    }
    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        latency_ms = atoi(buf);
    }
    return latency_ms;
}

int aml_audio_get_ddp_frame_size()
{
    int frame_size = DDP_FRAME_SIZE;
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    char *prop_name = "media.audio.hal.frame_size";
    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        frame_size = atoi(buf);
    }
    return frame_size;
}

bool is_stream_using_mixer(struct aml_stream_out *out)
{
    return is_inport_valid(out->enInputPortType);
}

uint32_t out_get_outport_latency(const struct audio_stream_out *stream)
{
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    struct subMixing *sm = adev->sm;
    struct amlAudioMixer *audio_mixer = sm->mixerData;
    int frames = 0, latency_ms = 0;

#ifdef ENABLE_BT_A2DP
    if (out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) {
        return a2dp_out_get_latency(stream);
    }
#endif

    if (is_stream_using_mixer(out)) {
        int outport_latency_frames = mixer_get_outport_latency_frames(audio_mixer);

        if (outport_latency_frames <= 0)
            outport_latency_frames = out->config.period_size * out->config.period_count / 2;

        frames = outport_latency_frames;
        ALOGV("%s(), total frames %d", __func__, frames);
        latency_ms = (frames * 1000) / out->config.rate;
        ALOGV("%s(), latencyMs %d, rate %d", __func__, latency_ms,out->config.rate);
    }
    return latency_ms;
}

static int get_fmt_rate(int codec_type)
{
    int rate = 1;
    if( (codec_type == TYPE_EAC3) ||
        (codec_type == TYPE_DTS_HD_MA) ||
        (codec_type == TYPE_DTS_HD))
        rate = 4;
    else if (codec_type == TYPE_TRUE_HD)
        rate = 16;
    return rate;
}

uint32_t out_get_latency_frames(const struct audio_stream_out *stream)
{
    const struct aml_stream_out *out = (const struct aml_stream_out *)stream;
    snd_pcm_sframes_t frames = 0;
    uint32_t whole_latency_frames;
    int ret = 0;
    int codec_type;
    int mul = 1;

    if (!stream) {
        return 0;
    }

    codec_type = get_codec_type(out->hal_internal_format);

    if (out->dual_output_flag) {
        if (out->hal_internal_format == AUDIO_FORMAT_E_AC3)
            mul = 1;
    } else
        mul = get_fmt_rate(codec_type);

    whole_latency_frames = out->config.period_size * out->config.period_count;
    if (!out->pcm || !pcm_is_ready(out->pcm)) {
        return whole_latency_frames / mul;
    }
    ret = pcm_ioctl(out->pcm, SNDRV_PCM_IOCTL_DELAY, &frames);
    if (ret < 0) {
        return whole_latency_frames / mul;
    }
    return frames / mul;
}

uint32_t out_get_ms12_latency_frames(const struct audio_stream_out *stream)
{
    const struct aml_stream_out *hal_out = (const struct aml_stream_out *)stream;
    snd_pcm_sframes_t frames = 0;
    struct snd_pcm_status status;
    uint32_t whole_latency_frames;
    int ret = 0;
    struct aml_audio_device *adev = hal_out->dev;
    struct aml_stream_out *ms12_out = adev->ms12_out;
    struct pcm_config *config = &adev->ms12_config;
    int mul = 1;

    if (ms12_out == NULL) {
        return 0;
    }
    if (adev->sink_format == AUDIO_FORMAT_E_AC3) {
        mul = 4;
    }

    whole_latency_frames = config->start_threshold;
    if (!ms12_out->pcm || !pcm_is_ready(ms12_out->pcm)) {
        return whole_latency_frames / mul;
    }

    ret = pcm_ioctl(ms12_out->pcm, SNDRV_PCM_IOCTL_STATUS, &status);
    if (ret < 0) {
        return whole_latency_frames / mul;
    }
    if (status.state != PCM_STATE_RUNNING && status.state != PCM_STATE_DRAINING) {
        return whole_latency_frames / mul;
    }

    ret = pcm_ioctl(ms12_out->pcm, SNDRV_PCM_IOCTL_DELAY, &frames);
    if (ret < 0) {
        return whole_latency_frames / mul;
    }
    return frames / mul;
}

int aml_audio_get_ms12_passthrough_latency(struct audio_stream_out *stream)
{
    char *prop_name = AUDIO_ATMOS_HDMI_PASSTHROUGH_PROPERTY;
    char buf[PROPERTY_VALUE_MAX];
    int latency_ms = 0;
    int ret = -1;
    if(is_bypass_ms12(stream))
    {
        latency_ms = VENDOR_AUDIO_MS12_PASSTHROUGH_LATENCY;
        ret = property_get(prop_name, buf, NULL);
        if (ret > 0) {
            latency_ms = atoi(buf);
        }
    }
    return latency_ms;
}

int aml_audio_get_spdif_tuning_latency(void)
{
    char *prop_name = "persist.vendor.audio.hal.spdif_ltcy_ms";
    char buf[PROPERTY_VALUE_MAX];
    int latency_ms = 0;
    int ret = -1;

    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        latency_ms = atoi(buf);
    }

    return latency_ms;
}
int aml_audio_get_speaker_latency_offset(audio_format_t fmt)
{
    char *prop_name = NULL;
    char buf[PROPERTY_VALUE_MAX];
    int latency_ms = 0;
    int ret = -1;

    switch (fmt) {
    case AUDIO_FORMAT_PCM_16_BIT:
        prop_name = AVSYNC_SPEAKER_PCM_LATENCY_PROPERTY;
        latency_ms = AVSYNC_SPEAKER_PCM_LATENCY;
        break;
    case AUDIO_FORMAT_AC3:
        prop_name = AVSYNC_SPEAKER_DD_LATENCY_PROPERTY;
        latency_ms = AVSYNC_SPEAKER_DD_LATENCY;
        break;
    case AUDIO_FORMAT_E_AC3:
        prop_name = AVSYNC_SPEAKER_DDP_LATENCY_PROPERTY;
        latency_ms = AVSYNC_SPEAKER_DDP_LATENCY;
        break;
    case AUDIO_FORMAT_AC4:
        prop_name = AVSYNC_SPEAKER_AC4_LATENCY_PROPERTY;
        latency_ms = AVSYNC_SPEAKER_AC4_LATENCY;
        break;

    case AUDIO_FORMAT_MAT:
        prop_name = AVSYNC_SPEAKER_MAT_LATENCY_PROPERTY;
        latency_ms = AVSYNC_SPEAKER_MAT_LATENCY;
    break;

    default:
        ALOGE("%s(), unsupported audio format : %#x", __func__, fmt);
        return 0;
    }

    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        latency_ms = atoi(buf);
    }

    return latency_ms;
}


int aml_audio_get_atmos_hdmi_latency_offset(audio_format_t fmt,int bypass)
{
    char *prop_name = AUDIO_ATMOS_HDMI_LANTCY_PROPERTY;
    char buf[PROPERTY_VALUE_MAX];
    int latency_ms = 0;
    int ret = -1;
    if (!bypass&&fmt)
    {
        latency_ms = AUDIO_ATMOS_HDMI_AUTO_LATENCY;
        ret = property_get(prop_name, buf, NULL);
        if (ret > 0)
        {
            latency_ms = atoi(buf);
        }
    }
    return latency_ms;
}

int aml_audio_get_hdmi_latency_offset(audio_format_t fmt)
{
    char *prop_name = NULL;
    char buf[PROPERTY_VALUE_MAX];
    int latency_ms = 0;
    int ret = -1;
    switch (fmt) {
    case AUDIO_FORMAT_PCM_16_BIT:
        prop_name = AVSYNC_HDMI_PCM_LATENCY_PROPERTY;
        latency_ms = AVSYNC_HDMI_PCM_LATENCY;
        break;
    case AUDIO_FORMAT_AC3:
        prop_name = AVSYNC_HDMI_DD_LATENCY_PROPERTY;
        latency_ms = AVSYNC_HDMI_DD_LATENCY;
        break;
    case AUDIO_FORMAT_E_AC3:
        prop_name = AVSYNC_HDMI_DDP_LATENCY_PROPERTY;
        latency_ms = AVSYNC_HDMI_DDP_LATENCY;
        break;
    case AUDIO_FORMAT_MAT:
        prop_name = AVSYNC_HDMI_MAT_LATENCY_PROPERTY;
        latency_ms = AVSYNC_HDMI_MAT_LATENCY;
        break;

    case  AUDIO_FORMAT_AC4:
        prop_name = AVSYNC_HDMI_AC4_LATENCY_PROPERTY;
        latency_ms = AVSYNC_HDMI_AC4_LATENCY;
        break;

    default:
        ALOGE("%s(), unsupported audio format : %#x", __func__, fmt);
        return 0;
    }

    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        latency_ms = atoi(buf);
    }

    return latency_ms;
}

int aml_audio_get_tvsrc_tune_latency(enum patch_src_assortion patch_src) {
    char *prop_name = NULL;
    char buf[PROPERTY_VALUE_MAX];
    int latency_ms = 0;
    int ret = -1;

    switch (patch_src)
    {
    case SRC_HDMIIN:
        prop_name = "persist.audio.tune_ms.hdmiin";
        break;
    case SRC_ATV:
        prop_name = "persist.audio.tune_ms.atv";
        break;
    case SRC_LINEIN:
        prop_name = "persist.audio.tune_ms.linein";
        break;
    default:
        ALOGE("%s(), unsupported audio patch source: %d", __func__, patch_src);
        return 0;
    }

    ret = property_get(prop_name, buf, NULL);
    if (ret > 0)
    {
        latency_ms = atoi(buf);
    }

    return latency_ms;
}

void audio_fade_func(void *buf,int fade_size,int is_fadein) {
    float fade_vol = is_fadein ? 0.0 : 1.0;
    int i = 0;
    float fade_step = is_fadein ? 1.0/(fade_size/4):-1.0/(fade_size/4);
    int16_t *sample = (int16_t *)buf;
    for (i = 0; i < fade_size/2; i += 2) {
        sample[i] = sample[i]*fade_vol;
        sample[i+1] = sample[i+1]*fade_vol;
        fade_vol += fade_step;
    }
    ALOGI("do fade %s done,size %d",is_fadein?"in":"out",fade_size);

}

void ts_wait_time_us(struct timespec *ts, uint32_t time_us)
{
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += (time_us / 1000000);
    ts->tv_nsec += (time_us * 1000);
    if (ts->tv_nsec >= 1000000000) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000;
    }
}

int cpy_16bit_data_with_gain(int16_t *dst, int16_t *src, int size_in_bytes, float vol)
{
    int size_in_short = size_in_bytes / 2;
    int i = 0;

    if (size_in_bytes % 2) {
        ALOGE("%s(), size inval %d", __func__, size_in_bytes);
        return -EINVAL;
    }

    if (vol > 1.0 || vol < 0) {
        ALOGE("%s(), inval vol %f, should in [0,1]", __func__, vol);
        return -EINVAL;
    }

    for (i = 0; i < size_in_short; i++) {
        dst[i] = src[i] * vol;
    }

    return 0;
}

static uint64_t timespec_ns(struct timespec tspec)
{
    return ((uint64_t)tspec.tv_sec * 1000000000 + tspec.tv_nsec);
}

uint64_t get_systime_ns(void)
{
    struct timespec tval;

    clock_gettime(CLOCK_MONOTONIC, &tval);

    return timespec_ns(tval);
}


// tval_new *must* later than tval_old
uint32_t tspec_diff_to_us(struct timespec tval_old,
        struct timespec tval_new)
{
    return (tval_new.tv_sec - tval_old.tv_sec) * 1000000
            + (tval_new.tv_nsec - tval_old.tv_nsec) / 1000;
}

int aml_set_thread_priority(char *pName, pthread_t threadId)
{
    struct sched_param  params = {0};
    int                 ret = 0;
    int                 policy = SCHED_FIFO; /* value:1 [pthread.h] */
    params.sched_priority = 5;
    ret = pthread_setschedparam(threadId, SCHED_FIFO, &params);
    if (ret != 0) {
        ALOGW("[%s:%d] set scheduled param error, ret:%#x", __func__, __LINE__, ret);
    }
    ret = pthread_getschedparam(threadId, &policy, &params);
    ALOGD("[%s:%d] thread:%s set priority, ret:%d policy:%d priority:%d",
            __func__, __LINE__, pName, ret, policy, params.sched_priority);
    return ret;
}


void aml_audio_switch_output_mode(int16_t *buf, size_t bytes, AM_AOUT_OutputMode_t mode)
{
    int16_t tmp;

    for (unsigned int i= 0; i < bytes / 2; i = i + 2) {
        switch (mode) {
            case AM_AOUT_OUTPUT_DUAL_LEFT:
                buf[i + 1] = buf[i];
                break;
            case AM_AOUT_OUTPUT_DUAL_RIGHT:
                buf[i] = buf[i + 1];
                break;
            case AM_AOUT_OUTPUT_SWAP:
                tmp = buf[i];
                buf[i] = buf[i + 1];
                buf[i + 1] = tmp;
                break;
            case AM_AOUT_OUTPUT_LRMIX:
                tmp = buf[i] / 2 + buf[i + 1] / 2;
                buf[i] = tmp;
                buf[i + 1] = tmp;
                break;
            default :
                break;
        }
    }
}

void * aml_audio_get_muteframe(audio_format_t output_format, int * frame_size, int bAtmos) {
    if (output_format == AUDIO_FORMAT_AC3) {
        *frame_size = sizeof(muted_frame_dd);
        return (void*)muted_frame_dd;
    } else if (output_format == AUDIO_FORMAT_E_AC3) {
        if (bAtmos) {
            *frame_size = sizeof(muted_frame_atmos);
            return (void*)muted_frame_atmos;
        } else {
            *frame_size = sizeof(muted_frame_ddp);
            return (void*)muted_frame_ddp;
        }
    } else {
        *frame_size = 0;
        return NULL;
    }
}


int aml_audio_get_dolby_drc_mode(int *drc_mode, int *drc_cut, int *drc_boost)
{
    char cEndpoint[PROPERTY_VALUE_MAX];
    int ret = 0;
    unsigned ac3_drc_control = (DDPI_UDC_COMP_LINE<<DRC_MODE_BIT)|(100<<DRC_HIGH_CUT_BIT)|(100<<DRC_LOW_BST_BIT);
    ac3_drc_control = get_sysfs_int("/sys/class/audiodsp/ac3_drc_control");

    if (!drc_mode || !drc_cut || !drc_boost)
        return -1;
    *drc_mode = ac3_drc_control&3;
    ALOGI("drc mode from sysfs %s\n",str_compmode[*drc_mode]);
    ret = property_get("ro.dolby.drcmode",cEndpoint,"");
    if (ret > 0) {
        *drc_mode = atoi(cEndpoint)&3;
        ALOGI("drc mode from prop %s\n",str_compmode[*drc_mode]);
    }
    *drc_cut  = (ac3_drc_control>>DRC_HIGH_CUT_BIT)&0xff;
    *drc_boost  = (ac3_drc_control>>DRC_LOW_BST_BIT)&0xff;
    ALOGI("dd+ drc mode %s,high cut %d pct,low boost %d pct\n",
        str_compmode[*drc_mode],*drc_cut, *drc_boost);
    return 0;
}

static int _earc_coding_type_mapping(int spdif_format)
{
    int r;
    enum {
        AUDIO_CODING_TYPE_UNDEFINED         = 0,
        AUDIO_CODING_TYPE_STEREO_LPCM       = 1,
        AUDIO_CODING_TYPE_MULTICH_2CH_LPCM  = 2,
        AUDIO_CODING_TYPE_MULTICH_8CH_LPCM  = 3,
        AUDIO_CODING_TYPE_MULTICH_16CH_LPCM = 4,
        AUDIO_CODING_TYPE_MULTICH_32CH_LPCM = 5,
        AUDIO_CODING_TYPE_HBR_LPCM          = 6,
        AUDIO_CODING_TYPE_AC3               = 7,
        AUDIO_CODING_TYPE_AC3_LAYOUT_B      = 8,
        AUDIO_CODING_TYPE_EAC3              = 9,
        AUDIO_CODING_TYPE_MLP               = 10,
        AUDIO_CODING_TYPE_DTS               = 11,
        AUDIO_CODING_TYPE_DTS_HD            = 12,
        AUDIO_CODING_TYPE_DTS_HD_MA         = 13,
        AUDIO_CODING_TYPE_SACD_6CH          = 14,
        AUDIO_CODING_TYPE_SACD_12CH         = 15,
        AUDIO_CODING_TYPE_PAUSE             = 16,
    };

    switch (spdif_format) {
        case AML_DOLBY_DIGITAL:
           r = AUDIO_CODING_TYPE_AC3;
           break;
        case AML_DOLBY_DIGITAL_PLUS:
           r = AUDIO_CODING_TYPE_EAC3;
           break;
        case AML_TRUE_HD:
           r = AUDIO_CODING_TYPE_MLP;
           break;
        case AML_DTS:
           r = AUDIO_CODING_TYPE_DTS;
           break;
        default:
           r = AUDIO_CODING_TYPE_STEREO_LPCM;
           break;
    }

    return r;
}

void aml_tinymix_set_spdif_format(audio_format_t output_format,struct aml_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    int aml_spdif_format = AML_STEREO_PCM;
    int spdif_mute = 0;
    if (output_format == AUDIO_FORMAT_AC3) {
        aml_spdif_format = AML_DOLBY_DIGITAL;
#ifdef ENABLE_DTV_PATCH
        audio_set_spdif_clock(stream, AML_DOLBY_DIGITAL);
#endif
    } else if (output_format == AUDIO_FORMAT_E_AC3) {
        aml_spdif_format = AML_DOLBY_DIGITAL_PLUS;
#ifdef ENABLE_DTV_PATCH
        audio_set_spdif_clock(stream, AML_DOLBY_DIGITAL_PLUS);
#endif
        // for BOX with ms12 continous mode, need DDP output
        if ((eDolbyMS12Lib == aml_dev->dolby_lib_type) && aml_dev->continuous_audio_mode && !aml_dev->is_TV) {
            // do nothing
        } else if ((eDolbyMS12Lib == aml_dev->dolby_lib_type) && !aml_dev->hdmitx_audio && !SUPPORT_EARC_OUT_HW) {
            // for TV, when DDP goes to SPDIF-A for ARC output, do nothing
        } else {
            spdif_mute = 1;
        }
    } else if (output_format == AUDIO_FORMAT_MAT) {
        aml_spdif_format = AML_TRUE_HD;
#ifdef ENABLE_DTV_PATCH
        audio_set_spdif_clock(stream, AML_TRUE_HD);
#endif
        if ((eDolbyMS12Lib == aml_dev->dolby_lib_type) && aml_dev->continuous_audio_mode && !aml_dev->is_TV) {
            // do nothing
        } else {
            spdif_mute = 1;
        }
    } else if (output_format == AUDIO_FORMAT_DTS) {
        aml_spdif_format = AML_DTS;
#ifdef ENABLE_DTV_PATCH
        audio_set_spdif_clock(stream, AML_DTS);
#endif
    } else {
        aml_spdif_format = AML_STEREO_PCM;
#ifdef ENABLE_DTV_PATCH
        audio_set_spdif_clock(stream, AML_STEREO_PCM);
#endif
    }
    aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_FORMAT, aml_spdif_format);
    aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_EARC_AUDIO_TYPE, _earc_coding_type_mapping(aml_spdif_format));
    if ((!aml_dev->spdif_force_mute) || (spdif_mute)) {
        aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_MUTE, spdif_mute);
    }
    ALOGI("%s tinymix AML_MIXER_ID_SPDIF_FORMAT %d,spdif mute %d",
          __FUNCTION__, aml_spdif_format, spdif_mute);
}

void aml_tinymix_set_spdifb_format(audio_format_t output_format,struct aml_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    int aml_spdif_format = AML_STEREO_PCM;
    int spdif_mute = 0;
    if (output_format == AUDIO_FORMAT_AC3) {
        aml_spdif_format = AML_DOLBY_DIGITAL;
    } else if (output_format == AUDIO_FORMAT_DTS) {
        aml_spdif_format = AML_DTS;
#ifdef ENABLE_DTV_PATCH
        audio_set_spdif_clock(stream, AML_DTS);
#endif
    } else {
        aml_spdif_format = AML_STEREO_PCM;
#ifdef ENABLE_DTV_PATCH
        audio_set_spdif_clock(stream, AML_STEREO_PCM);
#endif
    }
    aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_B_FORMAT, aml_spdif_format);
    aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_B_MUTE, 0);
    ALOGI("%s tinymix AML_MIXER_ID_SPDIF_B_FORMAT %d,spdif mute %d",
          __FUNCTION__, aml_spdif_format, spdif_mute);
}

void aml_audio_set_cpu23_affinity()
{
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(2, &cpuSet);
    CPU_SET(3, &cpuSet);
    int status = sched_setaffinity(0, sizeof(cpu_set_t), &cpuSet);
    if (status) {
        ALOGW("%s(), failed to set cpu affinity", __FUNCTION__);
    }
}
int aml_audio_get_video_latency(void)
{
    int latency_ms = 0;
    latency_ms = get_sysfs_int("/sys/class/video/video_display_latency");
    latency_ms = latency_ms/1000000;
    return latency_ms;
}

struct pcm_config update_earc_out_config(struct pcm_config *config)
{
    struct pcm_config earc_config;
    memset(&earc_config, 0, sizeof(struct pcm_config));
    earc_config.channels = 2;
    earc_config.rate = config->rate;
    earc_config.period_size = config->period_size;
    earc_config.period_count = config->period_count;
    earc_config.start_threshold = config->start_threshold;
    earc_config.format = config->format;
    return earc_config;
}

int continous_mode(struct aml_audio_device *adev)
{
    return adev->continuous_audio_mode;
}

bool direct_continous(struct audio_stream_out *stream)
{
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    if ((out->flags & AUDIO_OUTPUT_FLAG_DIRECT) && adev->continuous_audio_mode) {
        return true;
    } else {
        return false;
    }
}

bool primary_continous(struct audio_stream_out *stream)
{
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    if ((out->flags & AUDIO_OUTPUT_FLAG_PRIMARY) && adev->continuous_audio_mode) {
        return true;
    } else {
        return false;
    }
}

/* called when adev locked */
int dolby_stream_active(struct aml_audio_device *adev)
{
    int i = 0;
    int is_dolby = 0;
    struct aml_stream_out *out = NULL;
    for (i = 0 ; i < STREAM_USECASE_MAX; i++) {
        out = adev->active_outputs[i];
        if (out && (out->hal_internal_format == AUDIO_FORMAT_AC3
            || out->hal_internal_format == AUDIO_FORMAT_E_AC3
            || out->hal_internal_format == AUDIO_FORMAT_DOLBY_TRUEHD
            || out->hal_internal_format == AUDIO_FORMAT_AC4
            || out->hal_internal_format == AUDIO_FORMAT_MAT)) {
            is_dolby = 1;
            break;
        }
    }
    return is_dolby;
}

/* called when adev locked */
int hwsync_lpcm_active(struct aml_audio_device *adev)
{
    int i = 0;
    int is_hwsync_lpcm = 0;
    struct aml_stream_out *out = NULL;
    for (i = 0 ; i < STREAM_USECASE_MAX; i++) {
        out = adev->active_outputs[i];
        if (out && audio_is_linear_pcm(out->hal_internal_format) && (out->flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC)) {
            is_hwsync_lpcm = 1;
            break;
        }
    }
    return is_hwsync_lpcm;
}

struct aml_stream_out *direct_active(struct aml_audio_device *adev)
{
    int i = 0;
    struct aml_stream_out *out = NULL;
    for (i = 0 ; i < STREAM_USECASE_MAX; i++) {
        out = adev->active_outputs[i];
        if (out && (out->flags & AUDIO_OUTPUT_FLAG_DIRECT)) {
            return out;
        }
    }
    return NULL;
}
