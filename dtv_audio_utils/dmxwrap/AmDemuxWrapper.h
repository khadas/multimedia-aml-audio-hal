#ifndef AMDEMUX_WRAPPER_H
#define AMDEMUX_WRAPPER_H
//#include <VideodecWrapper.h>
#include <Mutex.h>
#include <TSPMessage.h>

extern "C" {

}

typedef int AM_DevSource_t;

/**\brief AV stream package format*/
typedef enum
{
    PFORMAT_ES = 0, /**< ES stream*/
    PFORMAT_PS,     /**< PS stream*/
    PFORMAT_TS,     /**< TS stream*/
    PFORMAT_REAL    /**< REAL file*/
} AM_AV_PFormat_t;

typedef int AM_AV_AFormat_t;
typedef int AM_AV_VFormat_t;

typedef enum
{
    AM_AV_NO_DRM,
    AM_AV_DRM_WITH_SECURE_INPUT_BUFFER, /**< Use for HLS, input buffer is clear and protected*/
    AM_AV_DRM_WITH_NORMAL_INPUT_BUFFER  /**< Use for IPTV, input buffer is normal and scramble*/
} AM_AV_DrmMode_t;

typedef struct Am_DemuxWrapper_OpenPara
{
    int device_type;              // dmx ,dsc,av
    int dev_no;
    AM_AV_VFormat_t  vid_fmt;     /**< Video format*/
    AM_AV_AFormat_t  aud_fmt;     /**< Audio format*/
    AM_AV_PFormat_t  pkg_fmt;     /**< Package format*/
    int              vid_id;      /**< Video ID, -1 means no video data*/
    int              aud_id;      /**< Audio ID, -1 means no audio data*/
    int              sub_id;      /**< Subtitle ID, -i means no subtitle data*/
    int              sub_type;    /**< Subtitle type.*/
    int              cntl_fd;     /*control fd*/
    int              aud_fd;      // inject /dev/amstream_mpts or dev/amstream_mpts_schedfd  handle
    int              vid_fd;      // inject /dev/amstream_mpts or dev/amstream_mpts_schedfd  handle
    void *           dsc_fd;
    AM_AV_DrmMode_t  drm_mode;
} Am_DemuxWrapper_OpenPara_t;

typedef struct Am_TsPlayer_Input_buffer
{
   uint8_t * data;
   int size;  //input: data size ,output : the real input size
} Am_TsPlayer_Input_buffer_t;

typedef enum
{
    AM_Dmx_SUCCESS,
    AM_Dmx_ERROR,
    AM_Dmx_DEVOPENFAIL,
    AM_Dmx_SETSOURCEFAIL,
    AM_Dmx_NOT_SUPPORTED,
    AM_Dmx_CANNOT_OPEN_FILE,
    AM_Dmx_ERR_SYS,
    AM_Dmx_MAX,
} AM_DmxErrorCode_t;


struct mEsDataInfo {
    uint8_t *data;
    int size;
    int64_t pts;
};
enum {
    kWhatReadVideo,
    kWhatReadAudio,
};

class  AmDemuxWrapper {
public:
   AmDemuxWrapper();
   virtual ~AmDemuxWrapper();
   virtual  AM_DmxErrorCode_t AmDemuxWrapperOpen(Am_DemuxWrapper_OpenPara_t *para) = 0;
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetTSSource(Am_DemuxWrapper_OpenPara_t *para,const AM_DevSource_t src) = 0;
   virtual  AM_DmxErrorCode_t AmDemuxWrapperStart() = 0;
   virtual  AM_DmxErrorCode_t AmDemuxWrapperWriteData(Am_TsPlayer_Input_buffer_t* Pdata, int *pWroteLen, uint64_t timeout) = 0;
   virtual  AM_DmxErrorCode_t AmDemuxWrapperReadData(int pid, mEsDataInfo **mEsdata,uint64_t timeout) = 0; //???????
   virtual  AM_DmxErrorCode_t AmDemuxWrapperFlushData(int pid) = 0; //???
   virtual  AM_DmxErrorCode_t AmDemuxWrapperPause() = 0;
   virtual  AM_DmxErrorCode_t AmDemuxWrapperResume() = 0;
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetAudioParam(int aid, AM_AV_AFormat_t afmt, int security_mem_level) = 0;
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetAudioDescParam(int aid, AM_AV_AFormat_t afmt)= 0;
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetSubtitleParam(int sid, int stype) = 0;
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetVideoParam(int vid, AM_AV_VFormat_t vfmt) = 0;
   virtual  AM_DmxErrorCode_t AmDemuxWrapperGetStates (int * value , int statetype) = 0;
   virtual  AM_DmxErrorCode_t AmDemuxWrapperStop() = 0;
   virtual  AM_DmxErrorCode_t AmDemuxWrapperClose() = 0;
   virtual  AmDemuxWrapper* get(void) {
        return this;
   }
   virtual void AmDemuxSetNotify(const sp<TSPMessage> & msg) {
        (void) msg;
   }

 //  virtual sp<TSPMessage> dupNotify() const { return mNotify->dup();}
//protected:
  // virtual sp<TSPMessage> dupNotify() const { return mNotify->dup();}
  // sp<TSPMessage> mNotify;
//private:
   // sp<TSPMessage> mNotify;
};
#endif
