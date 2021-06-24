/*
**
** Copyright 2012, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**  @author   Hugo Hong
**  @version  1.0
**  @date     2018/04/01
**  @par function description:
**  - 1 bluetooth rc audio stream in base class
*/

#define LOG_TAG "AudioHAL:AudioStreamIn"
#include <utils/Log.h>

#include "AudioStreamIn.h"
#include "AudioHardwareInput.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <utils/String8.h>
#include <media/AudioParameter.h>

// for turning Remote mic on/off
#ifdef REMOTE_CONTROL_INTERFACE
#include <IRemoteControlService.h>
#endif

namespace android {

const audio_format_t AudioStreamIn::kAudioFormat = AUDIO_FORMAT_PCM_16_BIT;
const uint32_t AudioStreamIn::kChannelMask = AUDIO_CHANNEL_IN_MONO;
const char AudioStreamIn::kRemoteSocketPath[] = "/data/misc/bluedroid/.rc_ctrl";
int AudioStreamIn::m_fd = -1;
bool AudioStreamIn::mStandby = true;

AudioStreamIn::AudioStreamIn(AudioHardwareInput& owner)
    : mOwnerHAL(owner)
    , mCurrentDeviceInfo(NULL)
    , mRequestedSampleRate(0)
    , mDisabled(false)
    , mInputSource(AUDIO_SOURCE_VOICE_RECOGNITION)
    , mReadStatus(0)
{
    mCurrentDeviceInfo = mOwnerHAL.getBestDevice(mInputSource);
}

AudioStreamIn::~AudioStreamIn()
{
     if (mStandby) {
        closeRemoteService();
    }
}

// Perform stream initialization that may fail.
// Must only be called once at construction time.
status_t AudioStreamIn::set(struct audio_stream_in *stream,
                            audio_format_t *pFormat,
                            uint32_t *pChannelMask,
                            uint32_t *pRate)
{
    Mutex::Autolock _l(mLock);

    (void) stream;

    // Respond with a request for mono if a different format is given.
    if (*pChannelMask != kChannelMask) {
        *pChannelMask = kChannelMask;
        return BAD_VALUE;
    }

    if (*pFormat != kAudioFormat) {
        *pFormat = kAudioFormat;
        return BAD_VALUE;
    }

    mRequestedSampleRate = *pRate;

    return NO_ERROR;
}


uint32_t AudioStreamIn::getSampleRate()
{
    Mutex::Autolock _l(mLock);
    return mRequestedSampleRate;
}

status_t AudioStreamIn::setSampleRate(uint32_t rate)
{
    (void) rate;
    // this is a no-op in other audio HALs
    return NO_ERROR;
}

size_t AudioStreamIn::getBufferSize()
{
   Mutex::Autolock _l(mLock);

    size_t size = AudioHardwareInput::calculateInputBufferSize(
        mRequestedSampleRate, kAudioFormat, getChannelCount());
    return size;
}

uint32_t AudioStreamIn::getChannelMask()
{
    return kChannelMask;
}

audio_format_t AudioStreamIn::getFormat()
{
    return kAudioFormat;
}

status_t AudioStreamIn::setFormat(audio_format_t format)
{
    (void) format;
    // other audio HALs fail any call to this API (even if the format matches
    // the current format)
    return INVALID_OPERATION;
}

status_t AudioStreamIn::standby()
{
    Mutex::Autolock _l(mLock);
    return standby_l();
}

status_t AudioStreamIn::dump(int fd)
{
    (void) fd;
    return NO_ERROR;
}

status_t AudioStreamIn::setParameters(struct audio_stream* stream,
                                      const char* kvpairs)
{
    (void) stream;
    (void) kvpairs;

    return NO_ERROR;
}

char* AudioStreamIn::getParameters(const char* keys)
{
    (void) keys;
    return strdup("");
}

status_t AudioStreamIn::setGain(float gain)
{
    (void) gain;
    // In other HALs, this is a no-op and returns success.
    return NO_ERROR;
}

uint32_t AudioStreamIn::getInputFramesLost()
{
    return 0;
}

status_t AudioStreamIn::addAudioEffect(effect_handle_t effect)
{
    (void) effect;
    // In other HALs, this is a no-op and returns success.
    return 0;
}

status_t AudioStreamIn::removeAudioEffect(effect_handle_t effect)
{
    (void) effect;
    // In other HALs, this is a no-op and returns success.
    return 0;
}

ssize_t AudioStreamIn::read(void* buffer, size_t bytes)
{
    Mutex::Autolock _l(mLock);

    status_t status = NO_ERROR;

    (void) buffer;

    if (mStandby) {
        status = startInputStream_l();
        // Only try to start once to prevent pointless spew.
        // If mic is not available then read will return silence.
        // This is needed to prevent apps from hanging.
        mStandby = false;
        if (status != NO_ERROR) {
            mDisabled = true;
        }
    }

    return bytes;
}

void AudioStreamIn::setRemoteControlMicEnabled(bool flag)
{
#ifdef REMOTE_CONTROL_INTERFACE
    sp<IRemoteControlService> service = IRemoteControlService::getService();
    if (service == NULL) {
        ALOGE("%s: No RemoteControl service detected, ignoring\n", __func__);
        return;
    }
    service->setMicEnable(flag == true ? 1 : 0);
#else
    m_fd = openRemoteService();
    if (m_fd > 0) {
        char status = (flag == true ? 1 : 0);
        send(m_fd, &status, sizeof(status), MSG_NOSIGNAL);
    }
#endif
}

status_t AudioStreamIn::startInputStream_l()
{
    // Get the most appropriate device for the given input source, eg VOICE_RECOGNITION
    const AudioHotplugThread::DeviceInfo *deviceInfo = mOwnerHAL.getBestDevice(mInputSource);
    if (deviceInfo == NULL) {
        return INVALID_OPERATION;
    }

    ALOGD("AudioStreamIn::startInputStream_l, mRequestedSampleRate = %d", mRequestedSampleRate);

    // Turn on RemoteControl MIC if we are recording from it.
    if (deviceInfo->forVoiceRecognition) {
        setRemoteControlMicEnabled(true);
    }

    mCurrentDeviceInfo = deviceInfo;

    return NO_ERROR;
}

status_t AudioStreamIn::standby_l()
{
    if (mStandby) {
        return NO_ERROR;
    }

    // Turn OFF Remote MIC if we were recording from Remote.
    if (mCurrentDeviceInfo != NULL) {
        if (mCurrentDeviceInfo->forVoiceRecognition) {
            setRemoteControlMicEnabled(false);
        }
    }

    mCurrentDeviceInfo = NULL;
    mStandby = true;
    mDisabled = false;

    return NO_ERROR;
}

int AudioStreamIn::openRemoteService()
{
#ifdef REMOTE_CONTROL_INTERFACE
    return 0;
#else
    int ret = -1;
    int fd = m_fd;
    socklen_t alen;
    size_t namelen;
    struct sockaddr_un addr;

    if (fd > 0) return fd;

    ALOGD("%s connect socket%s",__FUNCTION__, kRemoteSocketPath);

    fd = socket(AF_LOCAL, SOCK_STREAM, 0);

    namelen = strlen(kRemoteSocketPath);
    if ((namelen + 1) > sizeof(addr.sun_path))
        goto done;

    /*
    * Note: The path in this case is *not* supposed to be
    */
    addr.sun_path[0] = 0;
    memcpy(addr.sun_path + 1, kRemoteSocketPath, namelen);
    addr.sun_family = AF_LOCAL;
    alen = namelen + offsetof(struct sockaddr_un, sun_path) + 1;
    ret = connect(fd, (struct sockaddr *)&addr, alen);
    if (ret < 0) {
        ALOGE("connect failed:%d", errno);
        goto done;
    }

    ALOGD("%s: fd=%d, ret=%d", __func__, fd, ret);
done:
    if (ret < 0) {
        if (fd > 0) close(fd);
        fd = -1;
    }
    return fd;
#endif
}

void AudioStreamIn::closeRemoteService() {
#ifdef REMOTE_CONTROL_INTERFACE
    //do nothing
#else
    ALOGD("%s: fd=%d", __func__, m_fd);
    if (m_fd > 0) {
        close(m_fd);
        m_fd = -1;
    }
#endif
}

}; // namespace android
