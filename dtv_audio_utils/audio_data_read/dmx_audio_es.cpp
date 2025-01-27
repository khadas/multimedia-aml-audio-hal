//#define LOG_NDEBUG 0
#define LOG_TAG "dmx_audio_es"
#include <RefBase.h>
#ifndef BUILD_LINUX
#include <cutils/trace.h>
#endif
#include <cutils/log.h>
#include <cutils/properties.h>
#include <AmDemuxWrapper.h>
#include <AmHwDemuxWrapper.h>
#include <AmHwMultiDemuxWrapper.h>
#include <dmx.h>
extern "C" {
#include <dmx_audio_es.h>
int aml_audio_get_debug_flag();
}
using namespace audio_dmx;

AM_Dmx_Audio_ErrorCode_t Open_Dmx_Audio (void **demux_handle ,int demux_id, int security_mem_level) {
    Am_DemuxWrapper_OpenPara_t avpara;
    AmHwMultiDemuxWrapper *demux_wrapper = NULL;
    ALOGI("init demux_id %d security_mem_level %d",demux_id, security_mem_level);
    Am_DemuxWrapper_OpenPara_t * pavpara = &avpara;
    pavpara->dev_no = demux_id;
    pavpara->security_mem_level = security_mem_level;
    demux_wrapper = new AmHwMultiDemuxWrapper();
    demux_wrapper->AmDemuxWrapperOpen(pavpara);
    demux_wrapper->AmDemuxWrapperSetTSSource (pavpara,AM_AV_TSSource_t(demux_id));
    *demux_handle = (void *)demux_wrapper;
    return AM_AUDIO_Dmx_SUCCESS;
}

AM_Dmx_Audio_ErrorCode_t Init_Dmx_Main_Audio(void *demux_handle,int fmt, int pid) {

    AmHwMultiDemuxWrapper * demux_wrapper = (AmHwMultiDemuxWrapper *)demux_handle;
    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s fmt %d pid %d ",__FUNCTION__, fmt, pid);
    demux_wrapper->AmDemuxWrapperOpenMain(pid,fmt);

    return AM_AUDIO_Dmx_SUCCESS;
}

AM_Dmx_Audio_ErrorCode_t Flush_Dmx_Audio(void *demux_handle) {
    AmHwMultiDemuxWrapper * demux_wrapper = (AmHwMultiDemuxWrapper *)demux_handle;
    if (demux_wrapper == NULL) {
        ALOGD("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s %d, handle:%p",__FUNCTION__, __LINE__, demux_handle);
    demux_wrapper->AmDemuxWrapperFlushData(demux_wrapper->filering_aud_pid);
    return AM_AUDIO_Dmx_SUCCESS;
}

AM_Dmx_Audio_ErrorCode_t Stop_Dmx_Main_Audio(void *demux_handle) {
    AmHwMultiDemuxWrapper * demux_wrapper = (AmHwMultiDemuxWrapper *)demux_handle;
    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s %d",__FUNCTION__, __LINE__);
    //Temporarily remove , continue  tracking by SWPL-62301
    demux_wrapper->AmDemuxWrapperStopMain();
    return AM_AUDIO_Dmx_SUCCESS;
}

AM_Dmx_Audio_ErrorCode_t Start_Dmx_Main_Audio(void *demux_handle) {
    AmHwMultiDemuxWrapper * demux_wrapper = (AmHwMultiDemuxWrapper *)demux_handle;
    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s %d",__FUNCTION__, __LINE__);
    demux_wrapper->AmDemuxWrapperStartMain();
    return AM_AUDIO_Dmx_SUCCESS;
}
AM_Dmx_Audio_ErrorCode_t Destroy_Dmx_Main_Audio(void *demux_handle) {
    AmHwMultiDemuxWrapper * demux_wrapper = (AmHwMultiDemuxWrapper *)demux_handle;
    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s %d",__FUNCTION__, __LINE__);
    demux_wrapper->AmDemuxWrapperCloseMain();
    return AM_AUDIO_Dmx_SUCCESS;
}

AM_Dmx_Audio_ErrorCode_t Init_Dmx_AD_Audio(void *demux_handle, int fmt, int pid, int pesmode) {
    AmHwMultiDemuxWrapper * demux_wrapper = (AmHwMultiDemuxWrapper *)demux_handle;
    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s fmt %d pid %d pesmode %d",__FUNCTION__, fmt, pid, pesmode);
    demux_wrapper->AmDemuxWrapperOpenAD(pid, fmt, pesmode);
    return AM_AUDIO_Dmx_SUCCESS;
}
AM_Dmx_Audio_ErrorCode_t Stop_Dmx_AD_Audio(void *demux_handle) {
    AmHwMultiDemuxWrapper * demux_wrapper = (AmHwMultiDemuxWrapper *)demux_handle;
    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s %d",__FUNCTION__, __LINE__);
    //Temporarily remove , continue  tracking by SWPL-62301
    demux_wrapper->AmDemuxWrapperStopAD();
    return AM_AUDIO_Dmx_SUCCESS;
}

AM_Dmx_Audio_ErrorCode_t Start_Dmx_AD_Audio(void *demux_handle) {
    AmHwMultiDemuxWrapper * demux_wrapper = (AmHwMultiDemuxWrapper *)demux_handle;
    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s %d",__FUNCTION__, __LINE__);
    demux_wrapper->AmDemuxWrapperStartAD();

    return AM_AUDIO_Dmx_SUCCESS;
}
AM_Dmx_Audio_ErrorCode_t Destroy_Dmx_AD_Audio(void *demux_handle) {
    AmHwMultiDemuxWrapper * demux_wrapper = (AmHwMultiDemuxWrapper *)demux_handle;
    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s %d",__FUNCTION__, __LINE__);
    demux_wrapper->AmDemuxWrapperCloseAD();
    return AM_AUDIO_Dmx_SUCCESS;
}

AM_Dmx_Audio_ErrorCode_t Close_Dmx_Audio(void *demux_handle) {
    AmHwMultiDemuxWrapper * demux_wrapper = (AmHwMultiDemuxWrapper *)demux_handle;
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s %d",__FUNCTION__, __LINE__);
    demux_wrapper->AmDemuxWrapperStop();
    demux_wrapper->AmDemuxWrapperClose();
    if (demux_wrapper)
        delete demux_wrapper;
    demux_wrapper = NULL;
    return (AM_Dmx_Audio_ErrorCode_t)ret;
}

AM_Dmx_Audio_ErrorCode_t Get_MainAudio_Es(void *demux_handle,struct mAudioEsDataInfo  **mAudioEsData) {
    AmHwMultiDemuxWrapper * demux_wrapper = (AmHwMultiDemuxWrapper *)demux_handle;
    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }

    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    if (!VALID_PID(demux_wrapper->filering_aud_pid)) {
        return AM_AUDIO_Dmx_ERROR;
    }
    ret = demux_wrapper->AmDemuxWrapperReadData(demux_wrapper->filering_aud_pid, (mEsDataInfo **)mAudioEsData,1);
    //ALOGV("get_audio_es_package ret %d mEsdata  %p",ret,*mAudioEsData);
    if (*mAudioEsData == NULL) {
        return AM_AUDIO_Dmx_ERROR;
    }
    if ((*mAudioEsData)->size == 0) {
        return AM_AUDIO_Dmx_ERROR;
    }
    return (AM_Dmx_Audio_ErrorCode_t)ret;
}

AM_Dmx_Audio_ErrorCode_t Get_ADAudio_Es(void *demux_handle, struct mAudioEsDataInfo  **mAudioEsData) {
    AmHwMultiDemuxWrapper * demux_wrapper = (AmHwMultiDemuxWrapper *)demux_handle;
    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    if (!VALID_PID(demux_wrapper->filering_aud_ad_pid)) {
        return AM_AUDIO_Dmx_ERROR;
    }

    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    ret = demux_wrapper->AmDemuxWrapperReadData(demux_wrapper->filering_aud_ad_pid, (mEsDataInfo **)mAudioEsData,1);
    //ALOGV("Get_ADAudio_Es ret %d mEsdata  %p",ret,*mAudioEsData);
    if (*mAudioEsData == NULL) {
        return AM_AUDIO_Dmx_ERROR;
    }
    if ((*mAudioEsData)->size == 0) {
        return AM_AUDIO_Dmx_ERROR;
    }
   // if (aml_audio_get_debug_flag() > 1)
   //     ALOGV("\n ADmEsdata->pts : %lld size:%d \n",(*mAudioEsData)->pts,(*mAudioEsData)->size);
    return (AM_Dmx_Audio_ErrorCode_t)ret;
}

AM_Dmx_Audio_ErrorCode_t Get_Audio_LastES_Apts(void *demux_handle , int64_t *last_queue_es_apts) {
    AmHwMultiDemuxWrapper * demux_wrapper = (AmHwMultiDemuxWrapper *)demux_handle;
    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }

    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    if (!VALID_PID(demux_wrapper->filering_aud_pid)) {
        return AM_AUDIO_Dmx_ERROR;
    }
    *last_queue_es_apts = demux_wrapper->last_queue_es_apts;
    ALOGV("Get_Audio_LastES_Apts  %0llx",*last_queue_es_apts);

    return (AM_Dmx_Audio_ErrorCode_t)ret;
}


