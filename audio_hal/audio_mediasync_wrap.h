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



#ifndef _AUDIO_MEDIASYNC_WRAP_H_
#define _AUDIO_MEDIASYNC_WRAP_H_

//#include "MediaSyncInterface.h"
typedef enum {
    MEDIA_SYNC_VMASTER = 0,
    MEDIA_SYNC_AMASTER = 1,
    MEDIA_SYNC_PCRMASTER = 2,
    MEDIA_SYNC_MODE_MAX = 255,
}sync_mode;

typedef enum {
    MEDIA_VIDEO = 0,
    MEDIA_AUDIO = 1,
    MEDIA_DMXPCR = 2,
    MEDIA_SUBTITLE = 3,
    MEDIA_COMMON = 4,
    MEDIA_TYPE_MAX = 255,
}sync_stream_type;

typedef enum {
    AM_MEDIASYNC_OK  = 0,                      // OK
    AM_MEDIASYNC_ERROR_INVALID_PARAMS = -1,    // Parameters invalid
    AM_MEDIASYNC_ERROR_INVALID_OPERATION = -2, // Operation invalid
    AM_MEDIASYNC_ERROR_INVALID_OBJECT = -3,    // Object invalid
    AM_MEDIASYNC_ERROR_RETRY = -4,             // Retry
    AM_MEDIASYNC_ERROR_BUSY = -5,              // Device busy
    AM_MEDIASYNC_ERROR_END_OF_STREAM = -6,     // End of stream
    AM_MEDIASYNC_ERROR_IO            = -7,     // Io error
    AM_MEDIASYNC_ERROR_WOULD_BLOCK   = -8,     // Blocking error
    AM_MEDIASYNC_ERROR_MAX = -254
} mediasync_result;


typedef enum {
    MEDIASYNC_KEY_HASAUDIO = 0,
    MEDIASYNC_KEY_HASVIDEO,
    MEDIASYNC_KEY_VIDEOLATENCY,
    MEDIASYNC_KEY_AUDIOFORMAT,
    MEDIASYNC_KEY_STARTTHRESHOLD,
    MEDIASYNC_KEY_ISOMXTUNNELMODE,
    MEDIASYNC_KEY_AUDIOCACHE,
    MEDIASYNC_KEY_VIDEOWORKMODE,
    MEDIASYNC_KEY_AUDIOMUTE,
    MEDIASYNC_KEY_SOURCETYPE,
    MEDIASYNC_KEY_ALSAREADY,
    MEDIASYNC_KEY_VSYNC_INTERVAL_MS,
    MEDIASYNC_KEY_VIDEOFRAME,
    MEDIASYNC_KEY_MAX = 255,
} mediasync_parameter;

typedef enum {
    MEDIASYNC_UNIT_MS = 0,
    MEDIASYNC_UNIT_US,
    MEDIASYNC_UNIT_PTS,
    MEDIASYNC_UNIT_MAX,
} mediasync_time_unit;

typedef enum {
    MEDIASYNC_AUDIO_UNKNOWN = 0,
    MEDIASYNC_AUDIO_NORMAL_OUTPUT,
    MEDIASYNC_AUDIO_DROP_PCM,
    MEDIASYNC_AUDIO_INSERT,
    MEDIASYNC_AUDIO_HOLD,
    MEDIASYNC_AUDIO_MUTE,
    MEDIASYNC_AUDIO_RESAMPLE,
    MEDIASYNC_AUDIO_ADJUST_CLOCK,
    MEDIASYNC_AUDIO_EXIT,
} audio_policy;

struct mediasync_audio_policy {
    audio_policy audiopolicy;
    int32_t  param1;
    int32_t  param2;
};

struct mediasync_audio_format {
    int samplerate;
    int datawidth;
    int channels;
    int format;
};

struct mediasync_audio_queue_info{
    int64_t apts;
    int size;
    int duration;
    mediasync_time_unit tunit;
    bool isworkingchannel;
    bool isneedupdate;
};



void* mediasync_wrap_create();

bool mediasync_wrap_allocInstance(void* handle, int32_t DemuxId,
        int32_t PcrPid,
        int32_t *SyncInsId);

bool mediasync_wrap_bindInstance(void* handle, uint32_t SyncInsId, 
										sync_stream_type streamtype);
bool mediasync_wrap_setSyncMode(void* handle, sync_mode mode);
bool mediasync_wrap_getSyncMode(void* handle, sync_mode *mode);
bool mediasync_wrap_setPause(void* handle, bool pause);
bool mediasync_wrap_getPause(void* handle, bool *pause);
bool mediasync_wrap_setStartingTimeMedia(void* handle, int64_t startingTimeMediaUs);
bool mediasync_wrap_clearAnchor(void* handle);
bool mediasync_wrap_updateAnchor(void* handle, int64_t anchorTimeMediaUs,
								int64_t anchorTimeRealUs,
								int64_t maxTimeMediaUs);
bool mediasync_wrap_setPlaybackRate(void* handle, float rate);
bool mediasync_wrap_getPlaybackRate(void* handle, float *rate);
bool mediasync_wrap_getMediaTime(void* handle, int64_t realUs,
								int64_t *outMediaUs,
								bool allowPastMaxTime);
bool mediasync_wrap_getRealTimeFor(void* handle, int64_t targetMediaUs, int64_t *outRealUs);
bool mediasync_wrap_getRealTimeForNextVsync(void* handle, int64_t *outRealUs);
bool mediasync_wrap_getTrackMediaTime(void* handle, int64_t *outMeidaUs);
bool mediasync_wrap_setParameter(void* handle, mediasync_parameter type, void* arg);
bool mediasync_wrap_getParameter(void* handle, mediasync_parameter type, void* arg);
bool mediasync_wrap_queueAudioFrame(void* handle, struct mediasync_audio_queue_info* info);
bool mediasync_wrap_AudioProcess(void* handle, int64_t apts, int64_t cur_apts,
                                 mediasync_time_unit tunit,
                                 struct mediasync_audio_policy* asyncPolicy);

bool mediasync_wrap_reset(void* handle);
void mediasync_wrap_destroy(void* handle);

bool mediasync_wrap_setUpdateTimeThreshold(void* handle, int64_t value);
bool mediasync_wrap_getUpdateTimeThreshold(void* handle, int64_t *value);

#endif
