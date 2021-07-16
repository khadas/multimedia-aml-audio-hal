#ifdef BUILD_LINUX
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <TSPMessage.h>
#include <AmDemuxWrapper.h>

AmDemuxWrapper::AmDemuxWrapper() {
}

AmDemuxWrapper:: ~AmDemuxWrapper() {
}
#endif