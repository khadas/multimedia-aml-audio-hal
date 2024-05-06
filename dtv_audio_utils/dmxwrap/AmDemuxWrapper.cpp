#ifdef BUILD_LINUX
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <AmDemuxWrapper.h>

AmDemuxWrapper::AmDemuxWrapper() {
}

AmDemuxWrapper:: ~AmDemuxWrapper() {
}

AM_DmxErrorCode_t AmDemuxWrapper::AmDemuxWrapperOpen(Am_DemuxWrapper_OpenPara_t *para){
 return AM_Dmx_SUCCESS;
}
AM_DmxErrorCode_t AmDemuxWrapper::AmDemuxWrapperSetTSSource(Am_DemuxWrapper_OpenPara_t *para,const AM_DevSource_t src){
 return AM_Dmx_SUCCESS;
}
AM_DmxErrorCode_t AmDemuxWrapper::AmDemuxWrapperStart(){
 return AM_Dmx_SUCCESS;
}
AM_DmxErrorCode_t AmDemuxWrapper::AmDemuxWrapperWriteData(Am_TsPlayer_Input_buffer_t* Pdata, int *pWroteLen, uint64_t timeout){
 return AM_Dmx_SUCCESS;
}
AM_DmxErrorCode_t AmDemuxWrapper::AmDemuxWrapperReadData(int pid, mEsDataInfo **mEsdata,uint64_t timeout){
 return AM_Dmx_SUCCESS;
}
AM_DmxErrorCode_t AmDemuxWrapper::AmDemuxWrapperFlushData(int pid){
 return AM_Dmx_SUCCESS;
}
AM_DmxErrorCode_t AmDemuxWrapper::AmDemuxWrapperPause(){
 return AM_Dmx_SUCCESS;
}
AM_DmxErrorCode_t AmDemuxWrapper::AmDemuxWrapperResume(){
 return AM_Dmx_SUCCESS;
}
AM_DmxErrorCode_t AmDemuxWrapper::AmDemuxWrapperSetAudioParam(int aid, AM_AV_AFormat_t afmt, int security_mem_level){
 return AM_Dmx_SUCCESS;
}
AM_DmxErrorCode_t AmDemuxWrapper::AmDemuxWrapperSetAudioDescParam(int aid, AM_AV_AFormat_t afmt){
 return AM_Dmx_SUCCESS;
}
AM_DmxErrorCode_t AmDemuxWrapper::AmDemuxWrapperSetSubtitleParam(int sid, int stype){
 return AM_Dmx_SUCCESS;
}
AM_DmxErrorCode_t AmDemuxWrapper::AmDemuxWrapperSetVideoParam(int vid, AM_AV_VFormat_t vfmt){
 return AM_Dmx_SUCCESS;
}
AM_DmxErrorCode_t AmDemuxWrapper::AmDemuxWrapperGetStates (int * value , int statetype){
 return AM_Dmx_SUCCESS;
}
AM_DmxErrorCode_t AmDemuxWrapper::AmDemuxWrapperStop(){
 return AM_Dmx_SUCCESS;
}
AM_DmxErrorCode_t AmDemuxWrapper::AmDemuxWrapperClose(){
 return AM_Dmx_SUCCESS;
}
#endif