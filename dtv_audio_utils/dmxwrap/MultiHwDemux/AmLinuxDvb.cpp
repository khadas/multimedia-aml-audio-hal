//#define printf_LEVEL 5

//#include <am_misc.h>
//#define LOG_NDEBUG 0
#define LOG_TAG "AmLinuxDvb"

#include <cutils/log.h>
#include <cutils/properties.h>

#include <stdio.h>

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
/*add for config define for linux dvb *.h*/
//#include <am_config.h>
#include <AmLinuxDvb.h>
#include <AmDmx.h>
#include <sys/eventfd.h>
#include <dmx.h>

#include <inttypes.h>

namespace audio_dmx {


#define UNUSED(x) (void)(x)
AmLinuxDvd::AmLinuxDvd() {
    ALOGI("AmLinuxDvd\n");
    mDvrFd = -1;
}

AmLinuxDvd::~AmLinuxDvd() {
    ALOGI("~AmLinuxDvd\n");
}

AM_ErrorCode_t AmLinuxDvd::dvb_open(AM_DMX_Device *dev) {
    DVBDmx_t *dmx;
    int i;

    if (!dev) {
        ALOGE("[%s:%d] dev is %p", dev);
        return AM_FAILURE;
    }

    dmx = (DVBDmx_t*)malloc(sizeof(DVBDmx_t));
    if (!dmx)
    {
        ALOGE("[%s:%d] not enough memory", __func__, __LINE__);
        return AM_DMX_ERR_NO_MEM;
    }

    snprintf(dmx->dev_name, sizeof(dmx->dev_name), "/dev/dvb0.demux%d", dev->dev_no);
    //snprintf(dmx->dev_name, sizeof(dmx->dev_name), DVB_DEMUX);
    for (i=0; i < DMX_FILTER_COUNT; i++)
        dmx->fd[i] = -1;
    dmx->evtfd = eventfd(0, 0);
    if (dmx->evtfd == -1)
        ALOGI("[%s:%d] eventfd error", __func__, __LINE__);

    dev->drv_data = dmx;
    ALOGI("[%s() end: %d]dev %p, dev_no %d, dmx %p, evtfd %d", __func__, __LINE__, dev, dev->dev_no, dmx, dmx->evtfd);
    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_close(AM_DMX_Device *dev) {
    if (dev) {
        DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
        int ret = close(dmx->evtfd);
        ALOGI("[%s: %d]dev %p, dmx %p, evtfd %d, ret %d", __func__, __LINE__, dev, dmx, dmx->evtfd, ret);
        free(dmx);
        return AM_SUCCESS;
    } else {
        ALOGE("[%s:%d] dev is %p", dev);
        return AM_FAILURE;
    }
}

AM_ErrorCode_t AmLinuxDvd::dvb_alloc_filter(AM_DMX_Device *dev, AM_DMX_Filter *filter) {
    if (dev && filter) {
        DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
        int fd;
        fd = open(dmx->dev_name, O_RDWR);
        if (fd == -1)
        {
            ALOGE("cannot open \"%s\" (%s)", dmx->dev_name, strerror(errno));
            return AM_DMX_ERR_CANNOT_OPEN_DEV;
        }
        ALOGI("[%s ok: %d]  dev %p, dmx %p, filter %p, fd %d, name: %s", __func__, __LINE__, dev, dmx, filter, fd, dmx->dev_name);

        dmx->fd[filter->id] = fd;

        filter->drv_data = (void*)(long)fd;

        return AM_SUCCESS;
    } else {
        ALOGE("[%s:%d] dev is %p, filter is %p", dev, filter);
        return AM_FAILURE;
    }
}

AM_ErrorCode_t AmLinuxDvd::dvb_free_filter(AM_DMX_Device *dev, AM_DMX_Filter *filter) {
    if (dev && filter) {
        DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
        int fd = (long)filter->drv_data;

        int ret = close(fd);
        dmx->fd[filter->id] = -1;
        ALOGI("[%s:%d]dev %p, dmx %p, filter %p, fd %d, ret %d", __func__, __LINE__, dev, dmx, filter, fd, ret);

        return AM_SUCCESS;
    } else {
        ALOGE("[%s:%d] dev is %p, filter is %p", dev, filter);
        return AM_FAILURE;
    }
}

AM_ErrorCode_t AmLinuxDvd::dvb_get_stc(AM_DMX_Device *dev, AM_DMX_Filter *filter) {
    int fd = (long)filter->drv_data;
    int ret;
    struct dmx_stc stc;
    int i = 0;

    UNUSED(dev);

    for (i = 0; i < 3; i++) {
        memset(&stc, 0, sizeof(struct dmx_stc));
        stc.num = i;
        ret = ioctl(fd, DMX_GET_STC, &stc);
        if (ret == 0) {
            ALOGI("get stc num %d: base:0x%0x, stc:0x%llx, fd %d\n", stc.num, stc.base, stc.stc, fd);
        } else {
            ALOGE("get stc %d, fail. fd %d, ret %d", i, fd, ret);
        }
    }
    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_set_sec_filter(AM_DMX_Device *dev, AM_DMX_Filter *filter, const struct dmx_sct_filter_params *params) {
    struct dmx_sct_filter_params p;
    int fd = (long)filter->drv_data;
    int ret;

    UNUSED(dev);

    p = *params;
    /*
    if (p.filter.mask[0] == 0) {
        p.filter.filter[0] = 0x00;
        p.filter.mask[0]   = 0x80;
    }
    */
    ret = ioctl(fd, DMX_SET_FILTER, &p);
    if (ret == -1)
    {
        ALOGE("set section filter failed (%s)", strerror(errno));
        return AM_DMX_ERR_SYS;
    }
    ALOGI("[%s end: %d]dev %p, filter %p", __func__, __LINE__, dev, filter);

    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_set_pes_filter(AM_DMX_Device *dev, AM_DMX_Filter *filter, const struct dmx_pes_filter_params *params) {
    int fd = (long)filter->drv_data;
    int ret;

    UNUSED(dev);

    fcntl(fd,F_SETFL,O_NONBLOCK);

    ret = ioctl(fd, DMX_SET_PES_FILTER, params);
    if (ret == -1)
    {
        ALOGE("set section filter failed (%s)", strerror(errno));
        return AM_DMX_ERR_SYS;
    }
    ALOGI("[%s:%d] success. dev %p, filter %p, fd %d", __FUNCTION__, __LINE__, dev, filter, fd);
    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_enable_filter(AM_DMX_Device *dev, AM_DMX_Filter *filter, bool enable) {
    int fd = (long)filter->drv_data;
    int ret;

    UNUSED(dev);

    if (enable)
        ret = ioctl(fd, DMX_START, 0);
    else
        ret = ioctl(fd, DMX_STOP, 0);

    if (ret == -1)
    {
        ALOGE("start filter(%p) fd %d failed (%s)", filter, fd, strerror(errno));
        return AM_DMX_ERR_SYS;
    }
    ALOGI("[%s end: %d] filter %p, fd %d, enable %d", __func__, __LINE__, filter, fd, enable);

    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_set_buf_size(AM_DMX_Device *dev, AM_DMX_Filter *filter, int size) {
    int fd = (long)filter->drv_data;
    int ret;

    UNUSED(dev);

    ret = ioctl(fd, DMX_SET_BUFFER_SIZE, size);
    if (ret == -1)
    {
        ALOGE("set filter(%p) buffer size failed (%s)", filter, strerror(errno));
        return AM_DMX_ERR_SYS;
    }
    ALOGI("[%s:%d] filter %p, fd %d, size %d, ret %d", __func__, __LINE__, filter, fd, size, ret);

    return AM_SUCCESS;
}
AM_ErrorCode_t AmLinuxDvd::dvb_poll_exit(AM_DMX_Device *dev) {
    DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
    int64_t pad = 1;
    write(dmx->evtfd, &pad, sizeof(pad));
    ALOGI("[%s:%d]dev %p, dmx %p", __func__, __LINE__, dev, dmx);
    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_poll(AM_DMX_Device *dev, AM_DMX_FilterMask_t *mask, int timeout) {
    DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
    struct pollfd fds[DMX_FILTER_COUNT + 1];
    int fids[DMX_FILTER_COUNT + 1];
    int i, cnt = 0, ret;
    for (i = 0; i < DMX_FILTER_COUNT; i++)
    {
        if (dmx->fd[i] != -1)
        {
            fds[cnt].events = POLLIN|POLLERR;
            fds[cnt].fd     = dmx->fd[i];
            fids[cnt] = i;
            cnt++;
        }
    }

    if (!cnt)
        return AM_DMX_ERR_TIMEOUT;

    if (dmx->evtfd != -1) {
        fds[cnt].events = POLLIN|POLLERR;
        fds[cnt].fd     = dmx->evtfd;
        fids[cnt] = i;
        cnt++;
    }

    ret = poll(fds, cnt, timeout);
    if (ret <= 0)
    {
        ALOGI("timeout %d, ret %d",timeout, ret);
        return AM_DMX_ERR_TIMEOUT;
    }

    for (i = 0; i < cnt; i++)
    {
        if (fds[i].revents&(POLLIN|POLLERR))
        {
            AM_DMX_FILTER_MASK_SET(mask, fids[i]);
          //  ALOGV("fids[i] %d mask %d",fids[i],*mask);
        }
    }

    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvb_read(AM_DMX_Device *dev, AM_DMX_Filter *filter, uint8_t *buf, int *size) {
    int fd = (long)filter->drv_data;
    int len = *size;
    int ret;
    struct pollfd pfd;

    UNUSED(dev);

    if (fd == -1)
        return AM_DMX_ERR_NOT_ALLOCATED;

    pfd.events = POLLIN|POLLERR;
    pfd.fd     = fd;

    ret = poll(&pfd, 1, 0);
    if (ret <= 0)
        return AM_DMX_ERR_NO_DATA;

    ret = read(fd, buf, len);
    if (ret <= 0)
    {
        if (errno == ETIMEDOUT)
            return AM_DMX_ERR_TIMEOUT;
        ALOGE("read demux failed (%s) %d", strerror(errno), errno);
        return AM_DMX_ERR_SYS;
    }

    *size = ret;
    return AM_SUCCESS;
}

AM_ErrorCode_t AmLinuxDvd::dvr_open(void) {
    int ret;
    mDvrFd = open(DVB_DVR, O_WRONLY);
    if (mDvrFd == -1)
    {
        ALOGE("cannot open \"%s\" (%s)", DVB_DVR, strerror(errno));
        return -1;
    }
    ret = ioctl(mDvrFd, DMX_SET_INPUT, INPUT_DEMOD);
    if (ret < 0) {
        ALOGE("dvr_open ioctl failed \n");
    }
    ALOGI("[%s end: %d]mDvrFd %d", __func__, __LINE__, mDvrFd);
    return AM_SUCCESS;
}

int AmLinuxDvd::dvr_data_write(uint8_t *buf, int size,uint64_t timeout)
{
    int ret;
    int left = size;
    uint8_t *p = buf;
    int64_t nowUs = systemTime(SYSTEM_TIME_MONOTONIC) / 1000ll;
    timeout *= 10;
    while (left > 0)
    {
        //ALOGI("write start\n");
        ret = write(mDvrFd, p, left);
        //ALOGI("write end\n");
        if (ret == -1)
        {
            if (errno != EINTR)
            {
                ALOGE("Write DVR data failed: %s", strerror(errno));
                break;
            }
            ret = 0;
        } else {
        //  ALOGI("%s write cnt:%d\n",__FUNCTION__,ret);
        }

        left -= ret;
        p += ret;
        if (systemTime(SYSTEM_TIME_MONOTONIC) / 1000ll - nowUs > timeout) {
            ALOGE("dvr_data_write timeout %" PRIu64 " \n",timeout);
            break;
        }
    }

    return (size - left);
}

AM_ErrorCode_t AmLinuxDvd::dvr_close(void) {
    int ret = -1;
    if (mDvrFd > 0)
        ret = close(mDvrFd);
    ALOGI("[%s end: %d]mDvrFd %d, ret %d", __func__, __LINE__, mDvrFd, ret);
    return AM_SUCCESS;
}

#if 0
AM_ErrorCode_t AmLinuxDvd::dvb_set_source(AM_DMX_Device *dev, AM_DMX_Source_t src) {
    char buf[32];
    char *cmd;

    snprintf(buf, sizeof(buf), "/sys/class/stb/demux%d_source", dev->dev_no);

    switch (src)
    {
        case AM_DMX_SRC_TS0:
            cmd = "ts0";
        break;
        case AM_DMX_SRC_TS1:
            cmd = "ts1";
        break;
#if defined(CHIP_8226M) || defined(CHIP_8626X)
        case AM_DMX_SRC_TS2:
            cmd = "ts2";
        break;
#endif
        case AM_DMX_SRC_TS3:
            cmd = "ts3";
        break;
        case AM_DMX_SRC_HIU:
            cmd = "hiu";
        break;
        case AM_DMX_SRC_HIU1:
            cmd = "hiu1";
        break;
        default:
            ALOGE("do not support demux source %d", src);
        return AM_DMX_ERR_NOT_SUPPORTED;
    }
    return 0;
    return AM_FileEcho(buf, cmd);
}
#endif
}
