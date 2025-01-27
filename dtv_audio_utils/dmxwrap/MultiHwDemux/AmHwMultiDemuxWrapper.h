#ifndef AM_HWMULTI_DEMUX_WRAPPER_H
#define AM_HWMULTI_DEMUX_WRAPPER_H

#include <List.h>
#include <Mutex.h>
#include <StrongPointer.h>
#include "AmDemuxWrapper.h"

using namespace android;

namespace audio_dmx {
#define ENV_DMX_LOW_MEM "LOW_MEM_PLATFORM"
#define AM_DMX_LOW_MEM_BUFFER_SIZE (384*1024)     //384K for zapper 128M mem
#define AM_DMX_DEFAULT_BUFFER_SIZE (1024*1024)    //1M

class AM_DMX_Device;
typedef void (*AM_Audio_AD_DataCb) (const unsigned char * data, int len, void * handle);
typedef struct aduserdata
{
   long long adpts;
   unsigned char fade;
   unsigned char pan;
}ST_Aduserdata ;
class  AmHwMultiDemuxWrapper : public AmDemuxWrapper{

public:
   AmHwMultiDemuxWrapper();
   virtual ~AmHwMultiDemuxWrapper();
   //void getEsData(AmHwMultiDemuxWrapper* mDemuxWrapper, int fid, const uint8_t *data, int len, void *user_data);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperOpen(Am_DemuxWrapper_OpenPara_t *para);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetTSSource(Am_DemuxWrapper_OpenPara_t *para,const AM_DevSource_t src);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperStart(void);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperWriteData(Am_TsPlayer_Input_buffer_t* Pdata, int *pWroteLen, uint64_t timeout);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperReadData(int pid, mEsDataInfo **mEsData,uint64_t timeout); //???????
   virtual  AM_DmxErrorCode_t AmDemuxWrapperFlushData(int pid); //???
   virtual  AM_DmxErrorCode_t AmDemuxWrapperPause(void);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperResume(void);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetAudioParam(int aid, AM_AV_AFormat_t afmt);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetADAudioParam(int aid, AM_AV_AFormat_t afmt, int pemode);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetAudioDescParam(int aid, AM_AV_AFormat_t afmt);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetSubtitleParam(int sid, int stype);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetVideoParam(int vid, AM_AV_VFormat_t vfmt);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperGetStates (int * value , int statetype);
   virtual AM_DmxErrorCode_t  AmDemuxWrapperOpenAD(int aid, AM_AV_AFormat_t afmt, int pemode);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperStartAD();
   virtual  AM_DmxErrorCode_t AmDemuxWrapperStopAD();
   virtual  AM_DmxErrorCode_t AmDemuxWrapperCloseAD() ;
   virtual AM_DmxErrorCode_t  AmDemuxWrapperOpenMain(int aid, AM_AV_AFormat_t afmt);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperStartMain();
   virtual  AM_DmxErrorCode_t AmDemuxWrapperStopMain();
   virtual  AM_DmxErrorCode_t AmDemuxWrapperCloseMain() ;
   virtual  AM_DmxErrorCode_t AmDemuxWrapperStop(void);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperClose(void);
   AM_DmxErrorCode_t queueEsData(List<mEsDataInfo*>& mEsDataQueue,mEsDataInfo *mEsData) ;
   mEsDataInfo* dequeueEsData(List<mEsDataInfo*>& mEsDataQueue);
   AM_DmxErrorCode_t clearPendingEsData(List<mEsDataInfo*>& mEsDataQueue);
   Mutex mVideoEsDataQueueLock;
   Mutex mAudioEsDataQueueLock;
   Mutex mAudioADEsDataQueueLock;
   Mutex mDemuxHandleLock;
   List<mEsDataInfo*> mVideoEsDataQueue;
   List<mEsDataInfo*> mAudioEsDataQueue;
   List<mEsDataInfo*> mAudioADEsDataQueue;
   int              filering_aud_pid;
   int              filering_aud_ad_pid;
   int64_t          last_queue_es_apts;
   ST_Aduserdata     ADuserdata;
   int              adpesmode;
 private:
   sp<AM_DMX_Device> AmDmxDevice;
   // int mEsDataInfoSize;
   Am_DemuxWrapper_OpenPara_t  mDemuxPara;
};

}
#endif
