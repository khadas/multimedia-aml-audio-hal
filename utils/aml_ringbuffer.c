/*
 * hardware/amlogic/audio/utils/aml_ringbuffer.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 */

#define LOG_TAG "audio_hw_utils_ringbuf"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <cutils/log.h>
#include <aml_ringbuffer.h>
#include "tv_patch_format_parser.h"
#include "aml_malloc_debug.h"

/*************************************************
Function: get_write_space
Description: get the space can be written
Input: write_point: write pointer
       read_point:  read pointer
       buffer_size: total buffer size
Output:
Return: the space can be written in byte
*************************************************/
static int get_write_space(unsigned char *write_point, unsigned char *read_point,
        int buffer_size, int last_is_write)
{
    int bytes = 0;

    if (write_point > read_point) {
        bytes = buffer_size + read_point - write_point;
    } else if (write_point < read_point) {
        bytes = read_point - write_point;
    } else if (!last_is_write) {
        bytes = buffer_size;
    }

    return bytes;
}

/*************************************************
Function: get_read_space
Description: get the date space can be read
Input: write_point: write pointer
       read_point:  read pointer
       buffer_size: total buffer size
Output:
Return: the data space can be read in byte
*************************************************/
static size_t get_read_space(unsigned char *write_point, unsigned char *read_point,
        int buffer_size, int last_is_write) {
    int bytes = 0;

    if (write_point > read_point) {
        bytes = write_point - read_point;
    } else if (write_point < read_point) {
        bytes = buffer_size + write_point - read_point;
    } else if (last_is_write) {
        bytes = buffer_size;
    }

    return bytes;
}

/*************************************************
Function: write_to_buffer
Description: write data to ring buffer
Input: current_pointer: the current write pointer
       data:  data to be written
       bytes: the space of data to be written
       start_addr: dest buffer
       total_size: dest buffer size
Output:
Return: 0 for success
*************************************************/
static int write_to_buffer(unsigned char *current_pointer, const unsigned char *data, int bytes,
        unsigned char *start_addr, int total_size)
{
    int left_bytes = start_addr + total_size - current_pointer;

    if (left_bytes >= bytes) {
        memcpy(current_pointer, data, bytes);
    } else {
        memcpy(current_pointer, data, left_bytes);
        memcpy(start_addr, data + left_bytes, bytes - left_bytes);
    }

    return 0;
}

/*************************************************
Function: read_from_buffer
Description: read data to ring buffer
Input: current_pointer: the current read pointer
       buffer:  buffer for the data to be read
       bytes: the space of data to be read
       start_addr: dest buffer
       total_size: dest buffer size
Output: read data
Return: 0 for success
*************************************************/
static int read_from_buffer(unsigned char *current_pointer, unsigned char *buffer, int bytes,
        unsigned char *start_addr, int total_size)
{
    int left_bytes = start_addr + total_size - current_pointer;

    if (left_bytes >= bytes) {
        memcpy(buffer, current_pointer, bytes);
    } else {
        memcpy(buffer, current_pointer, left_bytes);
        memcpy(buffer + left_bytes, start_addr, bytes - left_bytes);
    }

    return 0;
}

/*************************************************
Function: update_pointer
Description: update read/write pointer of ring buffer
Input: current_pointer: the current read/write pointer
       bytes: data space has been written/read
       start_addr: ring buffer
       total_size: ring buffer size
Output:
Return: the updated pointer
*************************************************/
static inline void* update_pointer(unsigned char *current_pointer, int bytes,
        unsigned char *start_addr, int total_size)
{
    current_pointer += bytes;

    if (current_pointer >= start_addr + total_size) {
        current_pointer -= total_size;
    }

    return current_pointer;
}

/*************************************************
Function: ring_buffer_write
Description: write data to ring buffer
Input: rbuffer: the dest ring buffer
       data: data to be written
       bytes: data space in byte
       cover: whether or not to cover the data if over run
Output:
Return: data space has been written
*************************************************/
size_t ring_buffer_write(struct ring_buffer *rbuffer, const unsigned char* data, size_t bytes, int cover)
{
    struct ring_buffer *buf = rbuffer;
    size_t write_space, written_bytes;

    pthread_mutex_lock(&buf->lock);

    if (buf->start_addr == NULL || buf->rd == NULL || buf->wr == NULL || buf->size == 0) {
        ALOGE("%s, Buffer malloc fail!\n", __FUNCTION__);
        pthread_mutex_unlock(&buf->lock);
        return 0;
    }

    write_space = get_write_space(buf->wr, buf->rd, buf->size, buf->last_is_write);
    if (write_space < bytes) {
        if (UNCOVER_WRITE == cover) {
            written_bytes = write_space;
        } else {
            written_bytes = bytes;
        }
    } else {
        written_bytes = bytes;
    }

    write_to_buffer(buf->wr, data, written_bytes, buf->start_addr, buf->size);
    buf->wr = update_pointer(buf->wr, written_bytes, buf->start_addr, buf->size);
    if (written_bytes)
        buf->last_is_write = 1;

    pthread_mutex_unlock(&buf->lock);

    return written_bytes;
}

/*************************************************
Function: ring_buffer_read
Description: read data from ring buffer
Input: rbuffer: the source ring buffer
       buffer: buffer for the read data
       bytes: data space in byte
Output: data has been read
Return: data space has been read
*************************************************/
size_t ring_buffer_read(struct ring_buffer *rbuffer, unsigned char* buffer, size_t bytes)
{
    struct ring_buffer *buf = rbuffer;
    size_t readable_space, read_bytes;

    pthread_mutex_lock(&buf->lock);

    if (buf->start_addr == NULL || buf->rd == NULL || buf->wr == NULL
            || buf->size == 0) {
        ALOGE("%s, Buffer malloc fail!\n", __FUNCTION__);
        pthread_mutex_unlock(&buf->lock);
        return 0;
    }

    readable_space = get_read_space(buf->wr, buf->rd, buf->size, buf->last_is_write);
    if (readable_space < bytes) {
        read_bytes = readable_space;
    } else {
        read_bytes = bytes;
    }

    read_from_buffer(buf->rd, buffer, read_bytes, buf->start_addr, buf->size);
    buf->rd = update_pointer(buf->rd, read_bytes, buf->start_addr, buf->size);
    if (read_bytes)
        buf->last_is_write = 0;
    pthread_mutex_unlock(&buf->lock);

    return read_bytes;
}

/*************************************************
Function: ring_buffer_seek
Description: seek read or write pointer to add or
            reduce audio latency
Input: rbuffer: the source ring buffer
       bytes: seek space in byte,
              bytes > 0, reduce audio latency, move read pointer
              bytes < 0, add audio latency, move write pointer
Return: 0: do nothing for ringbuffer.
        bytes: moved space in byte
*************************************************/
int ring_buffer_seek(struct ring_buffer *rbuffer, int bytes)
{
    struct ring_buffer *buf = rbuffer;
    int seek_bytes = 0;

    pthread_mutex_lock(&buf->lock);

    if (buf->start_addr == NULL || buf->rd == NULL || buf->wr == NULL
            || buf->size == 0 || bytes == 0) {
        pthread_mutex_unlock(&buf->lock);
        return 0;
    }

    if (bytes > 0) {
        seek_bytes = get_read_space(buf->wr, buf->rd, buf->size, buf->last_is_write);
        if (seek_bytes > bytes) {
            seek_bytes = bytes;
        }
        if (seek_bytes > 0) {
            buf->last_is_write = 0;
        }
        buf->rd = update_pointer(buf->rd, seek_bytes, buf->start_addr, buf->size);
    } else {
        seek_bytes = get_write_space(buf->wr, buf->rd, buf->size, buf->last_is_write);
        bytes *= -1;
        if (seek_bytes > bytes) {
            seek_bytes = bytes;
        }
        buf->wr = update_pointer(buf->wr, seek_bytes, buf->start_addr, buf->size);
        seek_bytes *= -1;
    }

    pthread_mutex_unlock(&buf->lock);
    return seek_bytes;
}





/*************************************************
Function: ring_buffer_init
Description: initialize ring buffer
Input: rbuffer: the ring buffer to be initialized
       buffer_size: total size of ring buffer
Output:
Return: 0 for success, otherwise fail
*************************************************/
int ring_buffer_init(struct ring_buffer *rbuffer, int buffer_size)
{
    struct ring_buffer *buf = rbuffer;

    pthread_mutex_lock(&buf->lock);

    buf->size = buffer_size;
    buf->start_addr = aml_audio_malloc(buffer_size * sizeof(unsigned char));
    if (buf->start_addr == NULL) {
        ALOGD("%s, Malloc android out buffer error!\n", __FUNCTION__);
        pthread_mutex_unlock(&buf->lock);
        return -1;
    }

    memset(buf->start_addr, 0, buffer_size);
    buf->rd = buf->start_addr;
    buf->wr = buf->start_addr;
    buf->last_is_write = 0;
    pthread_mutex_unlock(&buf->lock);

    return 0;
}

/*************************************************
Function: ring_buffer_release
Description: release ring buffer
Input: rbuffer: the ring buffer to be released
Output:
Return: 0 for success, otherwise fail
*************************************************/
int ring_buffer_release(struct ring_buffer *rbuffer)
{
    struct ring_buffer *buf = rbuffer;

    pthread_mutex_lock(&buf->lock);

    if (buf->start_addr != NULL) {
        aml_audio_free(buf->start_addr);
        buf->start_addr = NULL;
    }

    buf->rd = NULL;
    buf->wr = NULL;
    buf->size = 0;
    buf->last_is_write = 0;

    pthread_mutex_unlock(&buf->lock);

    return 0;
}

/*************************************************
Function: ring_buffer_reset
Description: reset ring buffer
Input: rbuffer: the ring buffer to be reset
Output:
Return: 0 for success, otherwise fail
*************************************************/
int ring_buffer_reset(struct ring_buffer *rbuffer)
{
    struct ring_buffer *buf = rbuffer;

    pthread_mutex_lock(&buf->lock);
    memset(buf->start_addr, 0, buf->size);
    buf->rd = buf->wr = buf->start_addr;
    buf->last_is_write = 0;
    /*
     * if (buf->rd >= (buf->start_addr + buf->size))
     *    buf->rd -= buf->size;
     */
    pthread_mutex_unlock(&buf->lock);

    return 0;
}

/*************************************************
Function: ring_buffer_clear
Description: ring clear
Input: rbuffer: the ring buffer to be clear
Output:
Return: 0 for success, otherwise fail
*************************************************/
int ring_buffer_clear(struct ring_buffer *rbuffer)
{
    struct ring_buffer *buf = rbuffer;

    pthread_mutex_lock(&buf->lock);
    memset(buf->start_addr, 0, buf->size);
    pthread_mutex_unlock(&buf->lock);

    return 0;
}

/*************************************************
Function: ring_buffer_reset_size
Description: reset ring buffer and change the size
Input: rbuffer: the ring buffer to be reset
       buffer_size: new size for ring buffer
Output:
Return: 0 for success, otherwise fail
*************************************************/
int ring_buffer_reset_size(struct ring_buffer *rbuffer, int buffer_size)
{
    if (buffer_size > rbuffer->size) {
        ALOGW("resized buffer size exceed largest buffer size, max %d, cur %d\n", \
              rbuffer->size, buffer_size);
        ring_buffer_release(rbuffer);
        rbuffer->size = buffer_size;
        return ring_buffer_init(rbuffer, buffer_size);
    }

    ALOGI("reset buffer size from %d to %d\n", rbuffer->size, buffer_size);

    pthread_mutex_lock(&rbuffer->lock);

    rbuffer->size = buffer_size;
    memset(rbuffer->start_addr, 0, buffer_size);
    rbuffer->rd = rbuffer->start_addr;
    rbuffer->wr = rbuffer->start_addr;

    pthread_mutex_unlock(&rbuffer->lock);

    return 0;
}

/*************************************************
Function: get_buffer_read_space
Description: get the data space for reading in the buffer
Input: rbuffer: the ring buffer with data
Output:
Return: data space for success, otherwise < 0
*************************************************/
int get_buffer_read_space(struct ring_buffer *rbuffer)
{
    size_t read_space = 0;

    pthread_mutex_lock(&rbuffer->lock);
    if (rbuffer->start_addr == NULL || rbuffer->wr == NULL || rbuffer->rd == NULL
            || rbuffer->size == 0) {
        ALOGE("%s, Buffer malloc fail!\n", __FUNCTION__);
        pthread_mutex_unlock(&rbuffer->lock);
        return -1;
    }

    read_space = get_read_space(rbuffer->wr, rbuffer->rd, rbuffer->size, rbuffer->last_is_write);
    pthread_mutex_unlock(&rbuffer->lock);

    return read_space;
}

/*************************************************
Function: get_buffer_write_space
Description: get the space for writing in the buffer
Input: rbuffer: the ring buffer to be written
Output:
Return: data space for success, otherwise < 0
*************************************************/
int get_buffer_write_space(struct ring_buffer *rbuffer)
{
    size_t write_space = 0;

    pthread_mutex_lock(&rbuffer->lock);
    if (rbuffer->start_addr == NULL || rbuffer->wr == NULL || rbuffer->wr == NULL
        || rbuffer->size == 0) {
        ALOGE("%s, Buffer malloc fail!\n", __FUNCTION__);
        pthread_mutex_unlock(&rbuffer->lock);
        return -1;
    }

    write_space = get_write_space(rbuffer->wr, rbuffer->rd, rbuffer->size, rbuffer->last_is_write);
    pthread_mutex_unlock(&rbuffer->lock);

    return write_space;
}

// return value: The data size between sync word and read pointer of ring buffer
// if return value is -1, it means we can not find sync word in ring buffer
int find_61937_sync_word_position_in_ringbuffer(struct ring_buffer *rbuffer) {
    int read_space = 0;
    int pos_sync_word = -1;
    int sync_word_size = 4;
    unsigned char tmp_buf[4];

    pthread_mutex_lock(&rbuffer->lock);
    read_space = get_read_space(rbuffer->wr, rbuffer->rd, rbuffer->size, rbuffer->last_is_write);
    if (read_space >= sync_word_size) {
        // ring buffer  |-------------|---------------|-------------|
        //           start_addr      rd              wr
        if (rbuffer->rd < rbuffer->wr) {
            pos_sync_word = find_61937_sync_word((char*)rbuffer->rd, read_space);
        } else {
            // ring buffer  |----second_read_size----|---------------|---first_read_size---|
            //           start_addr                  wr              rd
            int first_read_size = rbuffer->size - (rbuffer->rd - rbuffer->start_addr);
            int second_read_size = rbuffer->wr - rbuffer->start_addr;
            //Check if the synchronization word is in first_read_size
            if (first_read_size >= sync_word_size) {
                pos_sync_word = find_61937_sync_word((char*)rbuffer->rd, first_read_size);
                if (pos_sync_word != -1) {
                    goto find_exit;
                }
            }
            //Check if the synchronization word is in second_read_size
            if (second_read_size >= sync_word_size) {
                pos_sync_word = find_61937_sync_word((char*)rbuffer->start_addr, second_read_size);
                if (pos_sync_word != -1) {
                    pos_sync_word += first_read_size;
                    goto find_exit;
                }
            }
            //Check if the synchronization word is in XXXX
            // ring buffer  |X------------|---------------|----------XXX|
            //           start_addr      rd              wr
            if (first_read_size >=3 && second_read_size >=1)
            {
                tmp_buf[0]= rbuffer->start_addr[rbuffer->size - 3];
                tmp_buf[1]= rbuffer->start_addr[rbuffer->size - 2];
                tmp_buf[2]= rbuffer->start_addr[rbuffer->size - 1];
                tmp_buf[3]= rbuffer->start_addr[0];
                pos_sync_word = find_61937_sync_word((char*)tmp_buf, 4);
                if (pos_sync_word != -1) {
                    pos_sync_word = first_read_size - 3;
                    goto find_exit;
                }
            }
            //Check if the synchronization word is in XXXX
            // ring buffer  |XX-----------|---------------|-----------XX|
            //           start_addr      rd              wr
            if (first_read_size >=2 && second_read_size >=2)
            {
                tmp_buf[0]= rbuffer->start_addr[rbuffer->size - 2];
                tmp_buf[1]= rbuffer->start_addr[rbuffer->size - 1];
                tmp_buf[2]= rbuffer->start_addr[0];
                tmp_buf[3]= rbuffer->start_addr[1];
                pos_sync_word = find_61937_sync_word((char*)tmp_buf, 4);
                if (pos_sync_word != -1) {
                    pos_sync_word = first_read_size - 2;
                    goto find_exit;
                }
            }
            //Check if the synchronization word is in XXXX
            // ring buffer  |XXX----------|---------------|------------X|
            //           start_addr      rd              wr
            if (first_read_size >=1 && second_read_size >=3)
            {
                tmp_buf[0]= rbuffer->start_addr[rbuffer->size - 1];
                tmp_buf[1]= rbuffer->start_addr[0];
                tmp_buf[2]= rbuffer->start_addr[1];
                tmp_buf[3]= rbuffer->start_addr[2];
                pos_sync_word = find_61937_sync_word((char*)tmp_buf, 4);
                if (pos_sync_word != -1) {
                    pos_sync_word = first_read_size - 1;
                    goto find_exit;
                }
            }
        }
    }

find_exit:
    pthread_mutex_unlock(&rbuffer->lock);
    return pos_sync_word;
}

/*************************************************
Function: ring_buffer_dump
Description: dump the ringbuffer status
Input: rbuffer: the ring buffer to be dumped
Output:
Return: NULL
*************************************************/
void ring_buffer_dump(struct ring_buffer *rbuffer)
{
    ALOGI("-buffer_size:%d", rbuffer->size);
    ALOGI("-buffer_avail:%d, buffer_space:%d", get_buffer_read_space(rbuffer), get_buffer_write_space(rbuffer));
}
#ifdef __cplusplus
}
#endif
