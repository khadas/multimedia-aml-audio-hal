#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h>

#include <linux/ioctl.h>
#define __force
#define __bitwise
#define __user
#include <sound/asound.h>

#include <tinyalsa/pcm.h>
//#include <tinyalsa/limits.h>

#define PCM_ERROR_MAX 128

/** A PCM handle.
 *  * @ingroup libtinyalsa-pcm
 *   */
struct pcm {
    /** The PCM's file descriptor */
    int fd;
    /** Flags that were passed to @ref pcm_open */
    unsigned int flags;
    /** Whether the PCM is running or not */
    int running:1;
    /** Whether or not the PCM has been prepared */
    int prepared:1;
    /** The number of underruns that have occured */
    int underruns;
    /** Size of the buffer */
    unsigned int buffer_size;
    /** The boundary for ring buffer pointers */
    unsigned int boundary;
    /** Description of the last error that occured */
    char error[PCM_ERROR_MAX];
    /** Configuration that was passed to @ref pcm_open */
    struct pcm_config config;
    struct snd_pcm_mmap_status *mmap_status;
    struct snd_pcm_mmap_control *mmap_control;
    struct snd_pcm_sync_ptr *sync_ptr;
    void *mmap_buffer;
    unsigned int noirq_frames_per_msec;
    /** The delay of the PCM, in terms of frames */
    long pcm_delay;
    /** The subdevice corresponding to the PCM */
    unsigned int subdevice;
};

int pcm_get_hw_ptr(struct pcm *pcm, unsigned long *hw_ptr,
                   struct timespec *tstamp)
{
    int frames;
    int rc;

    if (!pcm_is_ready(pcm))
        return -1;

    pcm->sync_ptr->flags = SNDRV_PCM_SYNC_PTR_APPL|SNDRV_PCM_SYNC_PTR_HWSYNC;
    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_SYNC_PTR, pcm->sync_ptr) < 0) {
        return -1;
    }

    if ((pcm->mmap_status->state != PCM_STATE_RUNNING) &&
            (pcm->mmap_status->state != PCM_STATE_DRAINING))
        return -1;

    *tstamp = pcm->mmap_status->tstamp;
    if (tstamp->tv_sec == 0 && tstamp->tv_nsec == 0)
        return -1;

    *hw_ptr = pcm->mmap_status->hw_ptr;

    return 0;
}

int pcm_set_monotonic_raw(struct pcm *pcm)
{
    int arg = SNDRV_PCM_TSTAMP_TYPE_MONOTONIC_RAW;
    int rc = ioctl(pcm->fd, SNDRV_PCM_IOCTL_TTSTAMP, &arg);
    if (rc < 0) {
        return -1;
    }

    return 0;
}

