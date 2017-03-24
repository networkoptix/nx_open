/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "NvJpegDecoder.h"
#include "NvLogging.h"
#include <string.h>
#include <malloc.h>
#include "unistd.h"
#include "stdlib.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ROUND_UP_4(num)  (((num) + 3) & ~3)

#define CAT_NAME "JpegDecoder"

NvJPEGDecoder::NvJPEGDecoder(const char *comp_name)
    :NvElement(comp_name, valid_fields)
{
    memset(&cinfo, 0, sizeof(cinfo));
    memset(&jerr, 0, sizeof(jerr));
    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_decompress(&cinfo);
}

NvJPEGDecoder *
NvJPEGDecoder::createJPEGDecoder(const char *comp_name)
{
    NvJPEGDecoder *jpegdec = new NvJPEGDecoder(comp_name);
    if (jpegdec->isInError())
    {
        delete jpegdec;
        return NULL;
    }
    return jpegdec;
}

NvJPEGDecoder::~NvJPEGDecoder()
{
    jpeg_destroy_decompress(&cinfo);
    CAT_DEBUG_MSG(comp_name << " (" << this << ") destroyed");
}

int
NvJPEGDecoder::decodeToFd(int &fd, unsigned char * in_buf,
        unsigned long in_buf_size, uint32_t &pixfmt, uint32_t &width,
        uint32_t &height)
{
    uint32_t pixel_format = 0;
    uint32_t buffer_id;

    if (in_buf == NULL || in_buf_size == 0)
    {
        COMP_ERROR_MSG("Not decoding because input buffer = NULL or size = 0");
        return -1;
    }

    buffer_id = profiler.startProcessing();

    cinfo.out_color_space = JCS_YCbCr;

    jpeg_mem_src(&cinfo, in_buf, in_buf_size);

    cinfo.out_color_space = JCS_YCbCr;

    /* Read file header, set default decompression parameters */
    (void) jpeg_read_header(&cinfo, TRUE);

    cinfo.out_color_space = JCS_YCbCr;
    cinfo.IsVendorbuf = TRUE;

    if (cinfo.comp_info[0].h_samp_factor == 2)
    {
        if (cinfo.comp_info[0].v_samp_factor == 2)
        {
            pixel_format = V4L2_PIX_FMT_YUV420M;
        }
        else
        {
            pixel_format = V4L2_PIX_FMT_YUV422M;
        }
    }
    else
    {
        if (cinfo.comp_info[0].v_samp_factor == 1)
        {
            pixel_format = V4L2_PIX_FMT_YUV444M;
        }
        else
        {
            pixel_format = V4L2_PIX_FMT_YUV422RM;
        }
    }

    jpeg_start_decompress (&cinfo);
    jpeg_read_raw_data (&cinfo, NULL, cinfo.comp_info[0].v_samp_factor * DCTSIZE);
    jpeg_finish_decompress(&cinfo);

    width = cinfo.image_width;
    height = cinfo.image_height;
    pixfmt = pixel_format;
    fd = cinfo.fd;

    COMP_DEBUG_MSG("Succesfully decoded Buffer fd=" << fd);

    profiler.finishProcessing(buffer_id, false);
    return 0;
}

int
NvJPEGDecoder::decodeToBuffer(NvBuffer ** buffer, unsigned char * in_buf,
        unsigned long in_buf_size, uint32_t *pixfmt, uint32_t * width,
        uint32_t * height)
{
    unsigned char **line[3];
    unsigned char *y[4 * DCTSIZE] = { NULL, };
    unsigned char *u[4 * DCTSIZE] = { NULL, };
    unsigned char *v[4 * DCTSIZE] = { NULL, };
    int i, j;
    int lines, v_samp[3];
    unsigned char *base[3], *last[3];
    int stride[3];
    NvBuffer *out_buf = NULL;
    uint32_t pixel_format = 0;
    uint32_t buffer_id;

    if (buffer == NULL)
    {
        COMP_ERROR_MSG("Not decoding because buffer = NULL");
        return -1;
    }

    if (in_buf == NULL || in_buf_size == 0)
    {
        COMP_ERROR_MSG("Not decoding because input buffer = NULL or size = 0");
        return -1;
    }

    buffer_id = profiler.startProcessing();

    cinfo.out_color_space = JCS_YCbCr;
    jpeg_mem_src(&cinfo, in_buf, in_buf_size);
    cinfo.out_color_space = JCS_YCbCr;

    (void) jpeg_read_header(&cinfo, TRUE);

    cinfo.out_color_space = JCS_YCbCr;

    if (cinfo.comp_info[0].h_samp_factor == 2)
    {
        if (cinfo.comp_info[0].v_samp_factor == 2)
        {
            pixel_format = V4L2_PIX_FMT_YUV420M;
        }
        else
        {
            pixel_format = V4L2_PIX_FMT_YUV422M;
        }
    }
    else
    {
        if (cinfo.comp_info[0].v_samp_factor == 1)
        {
            pixel_format = V4L2_PIX_FMT_YUV444M;
        }
        else
        {
            pixel_format = V4L2_PIX_FMT_YUV422RM;
        }
    }

    out_buf = new NvBuffer(pixel_format, cinfo.image_width,
            cinfo.image_height, 0);
    out_buf->allocateMemory();

    jpeg_start_decompress (&cinfo);

    line[0] = y;
    line[1] = u;
    line[2] = v;

    for (i = 0; i < 3; i++)
    {
        v_samp[i] = cinfo.comp_info[i].v_samp_factor;
        stride[i] = out_buf->planes[i].fmt.width;
        base[i] = out_buf->planes[i].data;
        last[i] = base[i] + (stride[i] * (out_buf->planes[i].fmt.height - 1));
    }

    for (i = 0; i < (int) cinfo.image_height; i += v_samp[0] * DCTSIZE)
    {
        for (j = 0; j < (v_samp[0] * DCTSIZE); ++j)
        {
            /* Y */
            line[0][j] = base[0] + (i + j) * stride[0];

            /* U,V */
            if (pixel_format == V4L2_PIX_FMT_YUV420M)
            {
                /* Y */
                line[0][j] = base[0] + (i + j) * stride[0];
                if ((line[0][j] > last[0]))
                    line[0][j] = last[0];
                /* U */
                if (v_samp[1] == v_samp[0]) {
                    line[1][j] = base[1] + ((i + j) / 2) * stride[1];
                } else if (j < (v_samp[1] * DCTSIZE)) {
                    line[1][j] = base[1] + ((i / 2) + j) * stride[1];
                }
                if ((line[1][j] > last[1]))
                    line[1][j] = last[1];
                /* V */
                if (v_samp[2] == v_samp[0]) {
                    line[2][j] = base[2] + ((i + j) / 2) * stride[2];
                } else if (j < (v_samp[2] * DCTSIZE)) {
                    line[2][j] = base[2] + ((i / 2) + j) * stride[2];
                }
                if ((line[2][j] > last[2]))
                    line[2][j] = last[2];
            }
            else
            {
                line[1][j] = base[1] + (i + j) * stride[1];
                line[2][j] = base[2] + (i + j) * stride[2];
            }
        }

        lines = jpeg_read_raw_data (&cinfo, line, v_samp[0] * DCTSIZE);
        if ((!lines))
        {
            COMP_DEBUG_MSG( "jpeg_read_raw_data() returned 0\n");
        }
    }

    jpeg_finish_decompress(&cinfo);
    if (width)
    {
        *width= cinfo.image_width;
    }
    if (height)
    {
        *height= cinfo.image_height;
    }
    if (pixfmt)
    {
        *pixfmt = pixel_format;
    }
    *buffer = out_buf;

    COMP_DEBUG_MSG("Succesfully decoded Buffer " << buffer);

    profiler.finishProcessing(buffer_id, false);

    return 0;
}
