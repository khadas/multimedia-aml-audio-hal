//#define LOG_NDEBUG 0
#define LOG_TAG "AmHwMultiDemuxWrapper"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#ifdef BUILD_LINUX
#include <cutils/log.h>
#include <cutils/properties.h>
#endif
#include <AmDemuxWrapper.h>
#include <AmHwMultiDemuxWrapper.h>
#include <AmDmx.h>
#include <dmx.h>
#include "RefBase.h"
extern "C" {
#include "aml_malloc_debug.h"
}

using namespace audio_dmx;
namespace audio_dmx {
static void getVideoEsData(AmHwMultiDemuxWrapper* mDemuxWrapper,int fid,const uint8_t *data, int len, void *user_data) {
//(void)mDemuxWrapper;
(void)fid;
//(void)data;
//(void)len;
(void)user_data;
    /*
    mEsDataInfo* mEsData = new mEsDataInfo;
    mEsData->data = (uint8_t*)malloc(len);
    memcpy(mEsData->data,data,len);
    {
        TSPMutex::Autolock l(mDemuxWrapper->mVideoEsDataQueueLock);
        mDemuxWrapper->queueEsData(mDemuxWrapper->mVideoEsDataQueue,mEsData);
    }

    sp<TSPMessage> msg = mDemuxWrapper->dupNotify();
    msg->setInt32("what", kWhatReadVideo);
    msg->post();
    */
    return;
}
#define  DEMUX_AUDIO_DUMP_PATH "/data/demux_audio.es"
#define  DEMUX_AD_AUDIO_DUMP_PATH "/data/demux_audio_ad.es"
static void dump_demux_data(void *buffer, int size, const char* file_name)
{
   if (property_get_bool("vendor.dvb.demux_audio_es.dump",false)) {
        FILE *fp1 = fopen(file_name, "a+");
        if (fp1) {
            int flen = fwrite((char *)buffer, 1, size, fp1);
            ALOGI("%s buffer %p size %d flen %d\n", __FUNCTION__, buffer, size,flen);
            fclose(fp1);
        }
    }
}

static void getAudioEsData(AmHwMultiDemuxWrapper* mDemuxWrapper, int fid, const uint8_t *data, int len, void *user_data) {
//(void)mDemuxWrapper;
(void)fid;
//(void)data;
//(void)len;
(void)user_data;

    mEsDataInfo* mEsData = (mEsDataInfo*)aml_audio_malloc(sizeof(mEsDataInfo));
    //ALOGI("getAudioEsData memorydebug %p\n",mEsData);
    dmx_non_sec_es_header *es_header = (struct dmx_non_sec_es_header *)(data);
    if (len == (es_header->len + sizeof(struct dmx_non_sec_es_header))) {
        const unsigned char *data_es  = data + sizeof(struct dmx_non_sec_es_header);
        mEsData->data = (uint8_t*)malloc(es_header->len);
        memcpy(mEsData->data, data_es, es_header->len);
        mEsData->size = es_header->len;
        mEsData->pts = es_header->pts;
        mDemuxWrapper->last_queue_es_apts = es_header->pts;
        mEsData->used_size = 0;
        dump_demux_data((void *)data_es, es_header->len, DEMUX_AUDIO_DUMP_PATH);
    } else {
        ALOGI("error es data len %d es_header->len %d",len, es_header->len);
        delete mEsData;
        mEsData = NULL;
        return;
    }

    {
        TSPMutex::Autolock l(mDemuxWrapper->mAudioEsDataQueueLock);
        mDemuxWrapper->queueEsData(mDemuxWrapper->mAudioEsDataQueue,mEsData);
    }
    //sp<TSPMessage> msg = mDemuxWrapper->dupNotify();
    //msg->setInt32("what", kWhatReadAudio);
    //msg->post();

}

static void getAudioADEsData(AmHwMultiDemuxWrapper* mDemuxWrapper, int fid, const uint8_t *data, int len, void *user_data) {
//(void)mDemuxWrapper;
(void)fid;
//(void)data;
//(void)len;
(void)user_data;
    if (0 == mDemuxWrapper->adpesmode)
    {
        mEsDataInfo* mEsData = (mEsDataInfo*)aml_audio_malloc(sizeof(mEsDataInfo));
        dmx_non_sec_es_header *es_header = (struct dmx_non_sec_es_header *)(data);
        if ( len == (es_header->len + sizeof(struct dmx_non_sec_es_header))) {
            const unsigned char *data_es  = data + sizeof(struct dmx_non_sec_es_header);
            mEsData->data = (uint8_t*)malloc(es_header->len);
            memcpy(mEsData->data, data_es, es_header->len);
            mEsData->size = es_header->len;
            mEsData->pts = es_header->pts;
            mEsData->used_size = 0;
            ALOGV("getAudioADEsData %d mEsData->size %d mEsData->pts %lld",len,mEsData->size,mEsData->pts);
            dump_demux_data((void *)data_es, es_header->len, DEMUX_AD_AUDIO_DUMP_PATH);
        }
        else {
            ALOGI("error es data len %d es_header->len %d",len, es_header->len);
            delete mEsData;
            mEsData = NULL;
            return;
        }
        {
            TSPMutex::Autolock l(mDemuxWrapper->mAudioADEsDataQueueLock);
            mDemuxWrapper->queueEsData(mDemuxWrapper->mAudioADEsDataQueue,mEsData);
        }
    }
    else
    {
        //AM_AD_Data_t *ad = (AM_AD_Data_t *)user_data;
        //AM_ErrorCode_t ret=AM_PES_Decode((mDemuxWrapper->peshandle), (uint8_t *)data, len);
        ST_Aduserdata *paddata=(ST_Aduserdata *)user_data;
        mEsDataInfo* mEsData = (mEsDataInfo*)aml_audio_malloc(sizeof(mEsDataInfo));
        mEsData->data = (uint8_t*)malloc(len);
        memcpy(mEsData->data, data, len);
        mEsData->size = len;
        mEsData->pts = paddata->adpts;
        mEsData->used_size = 0;
        mEsData->adfade= paddata->fade;
        mEsData->adpan= paddata->pan;
        ALOGV("\ngetADAudioEsData %d mEsData->size %d mEsData->pts %lld\n",len,mEsData->size,mEsData->pts);
        {
            TSPMutex::Autolock l(mDemuxWrapper->mAudioADEsDataQueueLock);
            mDemuxWrapper->queueEsData(mDemuxWrapper->mAudioADEsDataQueue,mEsData);
        }
    }

}

AmHwMultiDemuxWrapper::AmHwMultiDemuxWrapper() {
    ALOGI("AmHwMultiDemuxWrapper \n");
    AmDmxDevice = new AM_DMX_Device(this);
    filering_aud_pid  = 0x1fff;
    filering_aud_ad_pid  = 0x1fff;
    last_queue_es_apts = -1;
    mDemuxPara.vid_id = 0x1fff;


    mDemuxPara.aud_id = 0x1fff;
    mDemuxPara.aud_ad_id  = 0x1fff;
    mDemuxPara.aud_fmt  = -1;
    mDemuxPara.aud_ad_fmt = -1;
    mDemuxPara.aud_fd  = -1;
    mDemuxPara.aud_ad_fd = -1;

    mDemuxPara.sub_id = 0x1fff;
    mDemuxPara.vid_fmt = -1;
    mDemuxPara.sub_type = -1;
    mDemuxPara.drm_mode = AM_AV_NO_DRM;
    mDemuxPara.cntl_fd = -1;
}

AmHwMultiDemuxWrapper::~AmHwMultiDemuxWrapper() {
    ALOGI("~AmHwMultiDemuxWrapper \n");
    AmDmxDevice->AM_DMX_Close();
    AmDmxDevice  = NULL;
    filering_aud_pid  = 0x1fff;
    filering_aud_ad_pid  = 0x1fff;
    {
        TSPMutex::Autolock l(mVideoEsDataQueueLock);
        clearPendingEsData(mVideoEsDataQueue);
    }
    {
        TSPMutex::Autolock l(mAudioEsDataQueueLock);
        clearPendingEsData(mAudioEsDataQueue);
    }
    {
        TSPMutex::Autolock l(mAudioADEsDataQueueLock);
        clearPendingEsData(mAudioADEsDataQueue);
    }
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperOpen(Am_DemuxWrapper_OpenPara_t *mPara) {
    if (AmDmxDevice == NULL )  {
       ALOGE("AmDmxDevice is NULL");
       return AM_Dmx_ERROR;
    }
       ALOGI("AmDemuxWrapperOpen int");
    memcpy(&mDemuxPara,mPara,sizeof(Am_DemuxWrapper_OpenPara_t));
    AmDmxDevice->AM_DMX_Open(mDemuxPara.dev_no);

    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperSetTSSource(Am_DemuxWrapper_OpenPara_t *para,const AM_DevSource_t src) {
    (void) para;
    (void) src;
    mDemuxPara.dev_no = src;
       ALOGI("AmDemuxWrapperSetTSSource int, src:%d", src);

    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperStart(void) {

    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperWriteData(Am_TsPlayer_Input_buffer_t* Pdata, int *pWroteLen, uint64_t timeout) {
    if (AmDmxDevice->AM_DMX_WriteTs(Pdata->data,Pdata->size,timeout) < 0)
        return AM_Dmx_ERROR;

    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperReadData(int pid, mEsDataInfo **mEsData,uint64_t timeout) {
    //(void) pid;
    //(void) mEsData;
    (void) timeout;
    *mEsData = NULL;
    if (pid == mDemuxPara.vid_id) {
        TSPMutex::Autolock l(mVideoEsDataQueueLock);
        *mEsData = dequeueEsData(mVideoEsDataQueue);
    } else if (pid == mDemuxPara.aud_id){
        TSPMutex::Autolock l(mAudioEsDataQueueLock);
        *mEsData = dequeueEsData(mAudioEsDataQueue);
    } else if (pid == mDemuxPara.aud_ad_id) {
        TSPMutex::Autolock l(mAudioADEsDataQueueLock);
        *mEsData = dequeueEsData(mAudioADEsDataQueue);
    }
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperFlushData(int pid) {
    (void) pid;
    {
        TSPMutex::Autolock l(mAudioEsDataQueueLock);
        clearPendingEsData(mAudioEsDataQueue);
    }
    {
        TSPMutex::Autolock l(mAudioADEsDataQueueLock);
        clearPendingEsData(mAudioADEsDataQueue);
    }
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperPause(void) {
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperResume(void) {
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperSetADAudioParam(int aid, AM_AV_AFormat_t afmt, int pesmode ) {

   if (AmDmxDevice == NULL )  {
       ALOGE("AmDmxDevice is NULL");
       return AM_Dmx_ERROR;
    }

    struct dmx_pes_filter_params aparam;
    int aud_format;
    memset(&aparam, 0, sizeof(aparam));
    int fid_audio = 0;
    mDemuxPara.aud_ad_id = aid;
    mDemuxPara.aud_ad_fmt = afmt;
    switch (afmt) {
       case ACODEC_FMT_MPEG:
       case ACODEC_FMT_MPEG1:
       case ACODEC_FMT_MPEG2:
           aud_format = AUDIO_MPX;
           break;
       case ACODEC_FMT_AC3:
       case ACODEC_FMT_EAC3:
           aud_format = AUDIO_AC3;
           break;
       case ACODEC_FMT_AAC:
           aud_format = AUDIO_AAC_ADTS;
           break;
       case ACODEC_FMT_AAC_LATM:
          aud_format = AUDIO_AAC_LOAS;
          break;
       case ACODEC_FMT_DTS:
          aud_format = AUDIO_DTS;
          break;
       default:
          aud_format = AUDIO_UNKNOWN;
          break;
    }
    aparam.pid = aid;
    aparam.input = DMX_IN_FRONTEND;
    aparam.output = DMX_OUT_TAP;
    if (pesmode)
    {
        //aparam.pes_type = DMX_PES_TELETEXT0; //chuangcheng need fix bug on multi-demux
        aparam.pes_type = DMX_PES_AUDIO3;
        this->adpesmode=1;
    }
    else
    {
        aparam.pes_type = DMX_PES_AUDIO0;
        aparam.flags |= DMX_ES_OUTPUT;
        this->adpesmode=0;
    }
    /*
    if (mDemuxPara.security_mem_level == 10) {
        aparam.flags |= DMX_MEM_SEC_LEVEL1;
        aparam.flags |= ((aud_format & 0xff) << DMX_AUDIO_FORMAT_BIT);
    } else if (mDemuxPara.security_mem_level == 11) {
        aparam.flags |= DMX_MEM_SEC_LEVEL2;
        aparam.flags |= ((aud_format & 0xff) << DMX_AUDIO_FORMAT_BIT);
    } else if (mDemuxPara.security_mem_level == 12) {
        aparam.flags |= DMX_MEM_SEC_LEVEL3;
        aparam.flags |= ((aud_format & 0xff) << DMX_AUDIO_FORMAT_BIT);
    }
    */
    if( mDemuxPara.security_mem_level == DMX_MEM_SEC_LEVEL1 ||
        mDemuxPara.security_mem_level == DMX_MEM_SEC_LEVEL2 ||
        mDemuxPara.security_mem_level == DMX_MEM_SEC_LEVEL3 ||
        mDemuxPara.security_mem_level == DMX_MEM_SEC_LEVEL4 ||
        mDemuxPara.security_mem_level == DMX_MEM_SEC_LEVEL5 ||
        mDemuxPara.security_mem_level == DMX_MEM_SEC_LEVEL6 ||
        mDemuxPara.security_mem_level == DMX_MEM_SEC_LEVEL7 )
    {
        aparam.flags |= mDemuxPara.security_mem_level;
    }
    aparam.flags |= ((aud_format & 0xff) << DMX_AUDIO_FORMAT_BIT);
    //aparam.flags |= DMX_OUTPUT_RAW_MODE;
    AmDmxDevice->AM_DMX_AllocateFilter(&fid_audio);
    if (pesmode)
    {
        #define AD_BUF_SIZE (2*768*1024)
        memset(&(this->ADuserdata),0,sizeof(ST_Aduserdata));
        AmDmxDevice->AM_DMX_SetCallback(fid_audio, getAudioADEsData, &(this->ADuserdata));
        AmDmxDevice->AM_DMX_SetBufferSize(fid_audio, AD_BUF_SIZE);
    }
    else
    {
        AmDmxDevice->AM_DMX_SetCallback(fid_audio, getAudioADEsData, NULL);
        AmDmxDevice->AM_DMX_SetBufferSize(fid_audio, 1024 * 1024);
    }
    ALOGI("AM_DMX_SetPesFilter aparam.flags %0x",aparam.flags);
    AmDmxDevice->AM_DMX_SetPesFilter(fid_audio, &aparam);
    mDemuxPara.aud_ad_fd = fid_audio;
    ALOGI("aud_ad_fd %d",fid_audio);
    //AmDmxDevice->AM_DMX_StartFilter(fid_audio);
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperOpenAD(int aid, AM_AV_AFormat_t afmt, int pesmode ) {
   if (AmDmxDevice == NULL )  {
       ALOGE("AmDmxDevice is NULL");
       return AM_Dmx_ERROR;
    }
    AmDemuxWrapperSetADAudioParam(aid, afmt, pesmode);
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperStartAD() {
    if (AmDmxDevice == NULL )  {
       ALOGE("AmDmxDevice is NULL");
       return AM_Dmx_ERROR;
    }
    AmDmxDevice->AM_DMX_StartFilter(mDemuxPara.aud_ad_fd);
    ALOGI("mDemuxPara.aud_ad_fd %d",mDemuxPara.aud_ad_fd);
    filering_aud_ad_pid = mDemuxPara.aud_ad_id;
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperStopAD() {
    if (AmDmxDevice == NULL )  {
       ALOGE("AmDmxDevice is NULL");
       return AM_Dmx_ERROR;
    }
    AmDmxDevice->AM_DMX_StopFilter(mDemuxPara.aud_fd);
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperCloseAD() {
    if (AmDmxDevice == NULL )  {
       ALOGE("AmDmxDevice is NULL");
       return AM_Dmx_ERROR;
    }
    AmDmxDevice->AM_DMX_FreeFilter(mDemuxPara.aud_fd);
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperSetAudioParam(int aid, AM_AV_AFormat_t afmt) {

    if (AmDmxDevice == NULL )  {
        ALOGE("AmDmxDevice is NULL");
        return AM_Dmx_ERROR;
    }
    struct dmx_pes_filter_params aparam;
    int aud_format;
    memset(&aparam, 0, sizeof(aparam));
    int fid_audio = 0;
    mDemuxPara.aud_id = aid;
    mDemuxPara.aud_fmt = afmt;
    switch (afmt) {
       case ACODEC_FMT_MPEG:
       case ACODEC_FMT_MPEG1:
       case ACODEC_FMT_MPEG2:
           aud_format = AUDIO_MPX;
           break;
       case ACODEC_FMT_AC3:
       case ACODEC_FMT_EAC3:
           aud_format = AUDIO_AC3;
           break;
       case ACODEC_FMT_AAC:
           aud_format = AUDIO_AAC_ADTS;
           break;
       case ACODEC_FMT_AAC_LATM:
          aud_format = AUDIO_AAC_LOAS;
          break;
       case ACODEC_FMT_DTS:
          aud_format = AUDIO_DTS;
          break;
       default:
          aud_format = AUDIO_UNKNOWN;
          break;
    }
    aparam.pid = aid;
    aparam.pes_type = DMX_PES_AUDIO0;
    //aparam.pes_type = DMX_PES_VIDEO0;
    aparam.input = DMX_IN_FRONTEND;
    aparam.output = DMX_OUT_TAP;
    aparam.flags |= DMX_ES_OUTPUT;
    /*
    if (mDemuxPara.security_mem_level == 10) {
        aparam.flags |= DMX_MEM_SEC_LEVEL1;
        aparam.flags |= ((aud_format & 0xff) << DMX_AUDIO_FORMAT_BIT);
    } else if (mDemuxPara.security_mem_level == 11) {
        aparam.flags |= DMX_MEM_SEC_LEVEL2;
        aparam.flags |= ((aud_format & 0xff) << DMX_AUDIO_FORMAT_BIT);
    } else if (mDemuxPara.security_mem_level == 12) {
        aparam.flags |= DMX_MEM_SEC_LEVEL3;
        aparam.flags |= ((aud_format & 0xff) << DMX_AUDIO_FORMAT_BIT);
    }
    */
    if( mDemuxPara.security_mem_level == DMX_MEM_SEC_LEVEL1 ||
        mDemuxPara.security_mem_level == DMX_MEM_SEC_LEVEL2 ||
        mDemuxPara.security_mem_level == DMX_MEM_SEC_LEVEL3 ||
        mDemuxPara.security_mem_level == DMX_MEM_SEC_LEVEL4 ||
        mDemuxPara.security_mem_level == DMX_MEM_SEC_LEVEL5 ||
        mDemuxPara.security_mem_level == DMX_MEM_SEC_LEVEL6 ||
        mDemuxPara.security_mem_level == DMX_MEM_SEC_LEVEL7 )
    {
        aparam.flags |= mDemuxPara.security_mem_level;
    }
    aparam.flags |= ((aud_format & 0xff) << DMX_AUDIO_FORMAT_BIT);
    //aparam.flags |= DMX_OUTPUT_RAW_MODE;
    AmDmxDevice->AM_DMX_AllocateFilter(&fid_audio);
    AmDmxDevice->AM_DMX_SetCallback(fid_audio, getAudioEsData, NULL);
    unsigned int dmx_buffer_size = AM_DMX_DEFAULT_BUFFER_SIZE;
    const char *env = getenv(ENV_DMX_LOW_MEM);
    if (NULL != env)
    {
        dmx_buffer_size = AM_DMX_LOW_MEM_BUFFER_SIZE;
    }

    AmDmxDevice->AM_DMX_SetBufferSize(fid_audio, dmx_buffer_size);
    ALOGI("[%s():%d]AM_DMX_SetPesFilter aparam.flags %0x, dmx_buffer_size: 0x%x", __func__, __LINE__, aparam.flags, dmx_buffer_size);
    AmDmxDevice->AM_DMX_SetPesFilter(fid_audio, &aparam);
    mDemuxPara.aud_fd = fid_audio;
    ALOGI("fid_audio %d",fid_audio);
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperOpenMain(int aid, AM_AV_AFormat_t afmt) {
    if (AmDmxDevice == NULL )  {
       ALOGE("AmDmxDevice is NULL");
       return AM_Dmx_ERROR;
    }
    AmDemuxWrapperSetAudioParam(aid, afmt);
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperStartMain() {
     if (AmDmxDevice == NULL )  {
       ALOGE("AmDmxDevice is NULL");
       return AM_Dmx_ERROR;
    }
    AmDmxDevice->AM_DMX_StartFilter(mDemuxPara.aud_fd);
    filering_aud_pid = mDemuxPara.aud_id;
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperStopMain() {
    if (AmDmxDevice == NULL )  {
       ALOGE("AmDmxDevice is NULL");
       return AM_Dmx_ERROR;
    }
    AmDmxDevice->AM_DMX_StopFilter(mDemuxPara.aud_fd);
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperCloseMain() {
    if (AmDmxDevice == NULL )  {
       ALOGE("AmDmxDevice is NULL");
       return AM_Dmx_ERROR;
    }
    AmDmxDevice->AM_DMX_FreeFilter(mDemuxPara.aud_fd);
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperSetAudioDescParam(int aid, AM_AV_AFormat_t afmt) {
        (void) aid;
    (void) afmt;
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperSetSubtitleParam(int sid, int stype) {
    (void) sid;
    (void) stype;
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperSetVideoParam(int vid, AM_AV_VFormat_t vfmt) {
    ALOGI("AmDemuxWrapperSetVideoParam \n");
    struct dmx_pes_filter_params vparam;
    int fid_video = 0;
    mDemuxPara.vid_id = vid;
    mDemuxPara.vid_fmt = vfmt;
    vparam.pid = mDemuxPara.vid_id;
    vparam.pes_type = DMX_PES_VIDEO0;
    vparam.input = DMX_IN_FRONTEND;
    vparam.output = DMX_OUT_TAP;
    vparam.flags |= DMX_ES_OUTPUT;
    vparam.flags |= DMX_OUTPUT_RAW_MODE;

    AmDmxDevice->AM_DMX_AllocateFilter(&fid_video);
    AmDmxDevice->AM_DMX_SetCallback(fid_video, getVideoEsData, NULL);
    AmDmxDevice->AM_DMX_SetBufferSize(fid_video, 200*1024);
    AmDmxDevice->AM_DMX_SetPesFilter(fid_video, &vparam);
    AmDmxDevice->AM_DMX_StartFilter(fid_video);

    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperGetStates (int * value , int statetype) {
    (void) value;
    (void) statetype;
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperStop(void) {
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::AmDemuxWrapperClose(void) {
    return AM_Dmx_SUCCESS;
}

void AmHwMultiDemuxWrapper::AmDemuxSetNotify(const sp<TSPMessage> & msg) {
     mNotify = msg;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::queueEsData(List<mEsDataInfo*>& mEsDataQueue,mEsDataInfo *mEsData) {
   // TSPMutex::Autolock l(mEsDataQueueLock);
    mEsDataQueue.push_back(mEsData);
    return AM_Dmx_SUCCESS;
}

mEsDataInfo* AmHwMultiDemuxWrapper::dequeueEsData(List<mEsDataInfo*>& mEsDataQueue) {
    //Mutex::Autolock autoLock(mPacketQueueLock);
   //TSPMutex::Autolock l(mEsDataQueueLock);
    if (!mEsDataQueue.empty()) {
        mEsDataInfo *mEsData = *mEsDataQueue.begin();
        mEsDataQueue.erase(mEsDataQueue.begin());
        return mEsData;
    }
    return NULL;
}

AM_DmxErrorCode_t AmHwMultiDemuxWrapper::clearPendingEsData(List<mEsDataInfo*>& mEsDataQueue) {
   // TSPMutex::Autolock l(mEsDataQueueLock);
    List<mEsDataInfo *>::iterator it = mEsDataQueue.begin();
    while (it != mEsDataQueue.end()) {
        mEsDataInfo *mEsData = *it;
        free(mEsData->data);
        aml_audio_free(mEsData);
        ++it;
    }
    mEsDataQueue.clear();
    return AM_Dmx_SUCCESS;
}
}

