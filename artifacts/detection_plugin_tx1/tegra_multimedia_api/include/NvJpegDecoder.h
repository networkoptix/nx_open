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
 * <b>NVIDIA Eagle Eye API: Image Decode API</b>
 *
 * @b Description: This file declares the NvJpegDecoder APIs.
 */

#ifndef __NV_JPEG_DECODER_H__
#define __NV_JPEG_DECODER_H__

/**
 *
 * @defgroup ee_nvjpegdecoder_group Image Decode
 * @ingroup ee_nvimage_group
 *
 * The NvJPEGDecoder API provides functionality for decoding JPEG
 * images using libjpeg APIs.
 *
 * @{
 */

#ifndef TEGRA_ACCELERATE
/**
 * @c TEGRA_ACCELERATE must be defined to enable hardware-accelerated
 * JPEG decoding.
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
 * decoder.
 */
#define MAX_CHANNELS 3
#endif

/**
 * @brief Helper class for decoding JPEG images using libjpeg APIs.
 *
 * @c %NvJPEGDecoder uses the @c libjpeg APIs for decoding JPEG images. It
 * supports two methods for decoding:
 *
 * - Decode to a @c DMABUF and return its file descriptor (FD).
 * - Decode and write data to a NvBuffer object, i.e., software allocated
 *    memory (@c malloc).
 *
 * The jpeg decoder is capable of decoding YUV420, YUV422 and YUV444 jpeg images.
 *
 * @note Only the @c JCS_YCbCr (YUV420) color space is currently
 * supported.
 */
class NvJPEGDecoder:public NvElement
{
public:
    /**
     * Creates a new JPEG Decoder named @a comp_name.
     *
     * @return Reference to the newly created decoder object, or NULL
     *          in case of failure during initialization.
     */
    static NvJPEGDecoder *createJPEGDecoder(const char *comp_name);
     ~NvJPEGDecoder();

    /**
     * Decodes a JPEG image to hardware buffer memory.
     *
     * This method decodes JPEG images in hardware memory to a
     * specified width and size.
     *
     * @param[out] fd Indicates the file descriptor (FD) of the hardware buffer.
     * @param[in] in_buf Specifies a pointer to the memory containing
     *                   the JPEG image.
     * @param[in] in_buf_size Specifies the size of the input buffer in bytes.
     * @param[out] pixfmt Indicates a reference to the v4l2 pixel format
     *                    of the decoded image.
     * @param[out] width Indicates a reference to the width of the
     *                   decoded image in pixels.
     * @param[out] height Indicates a reference to the height of the
     *             decoded image in pixels.

     * @return 0 for success, -1 otherwise.
     */
    int decodeToFd(int &fd,
                     unsigned char *in_buf, unsigned long in_buf_size,
                     uint32_t &pixfmt, uint32_t &width, uint32_t &height);
    /**
     * Decodes a JPEG image to software buffer memory.
     *
     * This method decodes JPEG images in software memory to a
     * specified width and size.
     *
     * @note The @c decodeToBuffer method is slower than the
     * NvJPEGDecoder::decodeToFd method because it involves conversion
     * from hardware buffer memory to software buffer memory.
     *
     * @attention The application must free the NvBuffer object.
     *
     * @param[out] buffer Indicates @c %NvBuffer object to contain the
     *                    decoded image. The object is allocated by
     *                    the method and the buffer memory is
     *                    allocated using NvBuffer::allocateMemory.
     * @param[in] in_buf Specfies a pointer to the memory containing the JPEG.
     * @param[in] in_buf_size Specifies the size of the input buffer in bytes.
     * @param[out] pixfmt Indicates a pointer to the v4l2 pixel format
     *                    of the decoded image.
     * @param[out] width Indicates a pointer to the width of the decoded image
     *                   in pixels.
     * @param[out] height Indicates a pointer to the height of the decoded image
     *                    in pixels.
     * @return 0 for success, -1 otherwise.
     */
    int decodeToBuffer(NvBuffer ** buffer,
                         unsigned char *in_buf, unsigned long in_buf_size,
                         uint32_t *pixfmt, uint32_t *width, uint32_t *height);

private:

    NvJPEGDecoder(const char *comp_name);
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    static const NvElementProfiler::ProfilerField valid_fields =
            NvElementProfiler::PROFILER_FIELD_TOTAL_UNITS |
            NvElementProfiler::PROFILER_FIELD_LATENCIES;
};
#endif
/** @} */
