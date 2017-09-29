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

/**
 * @file
 * <b>NVIDIA Eagle Eye API: Image Encode API</b>
 *
 * @b This file declares the NvJpegEncoder APIs.
 */

#ifndef __NV_JPEG_ENCODER_H__
#define __NV_JPEG_ENCODER_H__

/**
 *
 * @defgroup ee_nvjpegencoder_group Image Encode
 * @ingroup ee_nvimage_group
 *
 * The NvJPEGEncoder API provides functionality for encoding JPEG
 * images using libjpeg APIs.
 *
 * @{
 */


#ifndef TEGRA_ACCELERATE
/**
 * @c TEGRA_ACCELERATE must be defined to enable hardware acceleration of
 * JPEG encoding.
 */
#define TEGRA_ACCELERATE
#endif

#include <stdio.h>
#include "jpeglib.h"
#include "NvElement.h"
#include "NvBuffer.h"

#ifndef MAX_CHANNELS
/**
 * Specifies the maximum number of channels to be used with the
 * encoder.
 */
#define MAX_CHANNELS 3
#endif

/**
 *
 * NvJpegEncoder uses the @c libjpeg APIs for encoding JPEG images. It
 * supports two methods for encoding:
 *
 * - Encode using a file descriptor (FD) of the MMAP buffer created by
 *    a V4L2 element. (Supports YUV420 and NV12 color formats)
 * - Encode using data pointers in NvBuffer object, i.e., software allocated
 *    memory (@c malloc). (Supports YUV420 color format)
 *
 * @note Only the JCS_YCbCr (YUV420) color space is currently
 * supported.
 */
class NvJPEGEncoder:public NvElement
{
public:
    /**
     * Create a new JPEG Encoder named @a comp_name.
     *
     * @return Reference to the newly created decoder object, or NULL
     *          in case of failure during initialization.
     */
    static NvJPEGEncoder *createJPEGEncoder(const char *comp_name);
     ~NvJPEGEncoder();

    /**
     *
     * Encodes a JPEG image from a file descriptor (FD) of hardware
     * buffer memory.
     *
     * The application may allocate the memory for storing the JPEG
     * image. If the allocation is less than what is required, @c
     * libjpeg allocates more memory. The @a out_buf pointer and @a
     * out_buf_size are updated accordingly.
     *
     * Supports YUV420 and NV12 formats.
     *
     * @attention The application must free the @a out_buf memory.
     *
     * @param[out] fd Indicates the file descriptor (FD) of the hardware buffer.
     * @param[out] color_space Indicates the color_space to use for encoding.
     * @param[in] out_buf Specifies a pointer to the memory for the JPEG image.
     * @param[in] out_buf_size Specifies the size of the output buffer in bytes.
     * @return 0 for success, -1 otherwise.
     */
    int encodeFromFd(int fd, J_COLOR_SPACE color_space,
                     unsigned char **out_buf, unsigned long &out_buf_size);

    /**
     * Encodes a JPEG image from software buffer memory.
     *
     * The application may allocate the memory for storing the JPEG
     * image. If the allocation is less than what is required, @c
     * libjpeg allocates more memory. The @a out_buf pointer and @a
     * out_buf_size are updated accordingly.
     *
     * The @c encodeFromBuffer method is slower than @c
     * NvJPEGEncoder::encodeFromFd because @c encodeFromBuffer
     * involves conversion from software buffer memory to hardware
     * buffer memory.
     *
     * Supports YUV420 format only.
     *
     * @attention The application must free the @a out_buf memory.
     *
     * @param[out] buffer Indicates the NvBuffer object to contain the encoded
                   image.
     * @param[out] color_space Indicates the color_space to use for encoding.
     * @param[in] out_buf Specifies a pointer to the memory for the JPEG image.
     * @param[in] out_buf_size Specifies the size of the output buffer in bytes.
     * @return 0 for success, -1 otherwise.
     */
    int encodeFromBuffer(NvBuffer & buffer, J_COLOR_SPACE color_space,
                         unsigned char **out_buf, unsigned long &out_buf_size);

    /**
     * Set the cropping rectangle to be used by the jpeg encoder. This method
     * should be called before #encodeFromFd or #encodeFromBuffer for the
     * cropping to take effect.
     *
     * @attention The jpeg encoder resets the crop paramaters after each call
     * to jpeg_finish_compress. Hence, this method must be called before every
     * call to #encodeFromFd or #encodeFromBuffer.
     *
     * @param[in] left Horizontal offset of the cropping rectangle, in pixels
     * @param[in] top  Vertical offset of the cropping rectangle, in pixels
     * @param[in] width Width of the cropping rectangle, in pixels
     * @param[in] height Height of the cropping rectangle, in pixels
     */
    void setCropRect(uint32_t left, uint32_t top, uint32_t width,
            uint32_t height);

private:

    NvJPEGEncoder(const char *comp_name);
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    static const NvElementProfiler::ProfilerField valid_fields =
            NvElementProfiler::PROFILER_FIELD_TOTAL_UNITS |
            NvElementProfiler::PROFILER_FIELD_LATENCIES;
};
#endif
/** @} */
