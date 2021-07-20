#ifndef TINYALSA_EXT_
#define TINYALSA_EXT_
#include <time.h>

extern int pcm_get_hw_ptr(struct pcm *pcm, unsigned long *hw_ptr,
                          struct timespec *tstamp);

extern int pcm_set_monotonic_raw(struct pcm *pcm);

#endif /* TINYALSA_EXT_ */

