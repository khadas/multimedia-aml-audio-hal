#ifndef __HBGDECODE_INFO_INTERFACE_H__#define __HBGDECODE_INFO_INTERFACE_H__#ifdef __cplusplusextern "C" {#endifvoid hbg_init();void hbg_stop_thread();void dumpData(char *path,void* buf,int len);int read_data_from_buffer(unsigned char*out ,int len);void send_start_audio_cmd(void);void send_stop_audio_cmd(void);#ifdef __cplusplus}#endif#endif