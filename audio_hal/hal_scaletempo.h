#ifndef _HAL_SCALETEMPO_H_
#define _HAL_SCALETEMPO_H_

typedef struct scale_tempo_buffer_s
{
    /** Channel count */
    unsigned    nchannel;

    /** Distance, in units of the chosen data type, between consecutive samples
     *  of the same channel. For DLB_BUFFER_OCTET_PACKED, this is in units of
     *  unsigned char. For interleaved audio data, set this equal to nchannel.
     *  For separate channel buffers, set this to 1.
     */
    unsigned    nstride;

    /** One of the DLB_BUFFER_* codes */
    int         data_type;

    /** Array of pointers to data, one for each channel. */
    void** ppdata;
} scale_tempo_buffer;

typedef struct aml_scaletempo_info {
    int sr ;   /** the decoded data samplerate*/
    int ch ;   /** the decoded data channels*/
    int sample_size; /**the decoded sample bit width*/
    int data_type;
    scale_tempo_buffer * inputbuffer;
    int input_samples;
    scale_tempo_buffer * outputbuffer;
    int output_samples;
} aml_scaletempo_info_t;

#endif

