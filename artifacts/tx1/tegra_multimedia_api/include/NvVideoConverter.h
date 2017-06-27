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
 * @file NvVideoConverter.h
 *
 * @brief Helper class for V4L2 Video Converter.
 */

#ifndef __NV_VIDEO_CONVERTER_H__
#define __NV_VIDEO_CONVERTER_H__

#include "NvV4l2Element.h"

/**
 * @brief Helper class for V4L2 Video Converter.
 *
 * Video converter can be used for color space conversion, scaling and
 * conversion between hardware buffer memory (V4L2_MEMORY_MMAP/
 * V4L2_MEMORY_DMABUF) and software buffer memory (V4L2_MEMORY_USERPTR).
 *
 * The video converter device node is \b "/dev/nvhost-vic". The category name
 * for encoder is \b "NVVIDCONV"
 *
 * Refer to @ref V4L2Conv for more information on the converter.
 */

class NvVideoConverter:public NvV4l2Element
{
public:

    /**
     * Create a new V4L2 Video Converter named \a name
     *
     * This function internally calls v4l2_open on the converter dev node
     * \b "/dev/nvhost-vic" and checks for \b V4L2_CAP_VIDEO_M2M_MPLANE
     * capability on the device. This method allows the caller to specify
     * additional flags with which the device should be opened.
     *
     * The device is opened in blocking mode which can be modified by passing
     * the @a O_NONBLOCK flag to this method.
     *
     * @returns Reference to the newly created converter object else \a NULL in
     *          case of failure during initialization.
     */
    static NvVideoConverter *createVideoConverter(const char *name, int flags = 0);

     ~NvVideoConverter();
    /**
     * Sets the format on the converter output plane.
     *
     * Calls VIDIOC_S_FMT ioctl internally on the capture plane.
     *
     * @param[in] pixfmt One of the raw V4L2 pixel formats
     * @param[in] width Width of the output buffers in pixels
     * @param[in] height Height of the output buffers in pixels
     * @param[in] layout Layout of the buffers in plane, one of
     *                   enum v4l2_nv_buffer_layout
     * @returns 0 for success, -1 for failure
     */
    int setCapturePlaneFormat(uint32_t pixfmt, uint32_t width, uint32_t height,
                              enum v4l2_nv_buffer_layout type);
    /**
     * Sets the format on the converter output plane.
     *
     * Calls VIDIOC_S_FMT ioctl internally on the output plane.
     *
     * @param[in] pixfmt One of the raw V4L2 pixel formats
     * @param[in] width Width of the output buffers in pixels
     * @param[in] height Height of the output buffers in pixels
     * @param[in] layout Layout of the buffers in plane, one of
     *                   enum v4l2_nv_buffer_layout
     * @returns 0 for success, -1 for failure
     */
    int setOutputPlaneFormat(uint32_t pixfmt, uint32_t width, uint32_t height,
                             enum v4l2_nv_buffer_layout type);

    /**
     * Set the buffer layout of the output plane buffers
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_VIDEO_CONVERT_OUTPUT_PLANE_LAYOUT. Must be called before
     * setFormat on any of the planes.
     *
     * @param[in] type Type of layout, one of enum v4l2_nv_buffer_layout
     *
     * @returns 0 for success, -1 for failure
     */
    int setOutputPlaneBufferLayout(enum v4l2_nv_buffer_layout type);

    /**
     * Set the buffer layout of the capture plane buffers
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_VIDEO_CONVERT_CAPTURE_PLANE_LAYOUT. Must be called before
     * setFormat on any of the planes.
     *
     * @param[in] type Type of layout, one of enum v4l2_nv_buffer_layout
     *
     * @returns 0 for success, -1 for failure
     */
    int setCapturePlaneBufferLayout(enum v4l2_nv_buffer_layout type);

    /**
     * Set the interpolation(filter) method used for scaling
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_VIDEO_CONVERT_INTERPOLATION_METHOD. Must be called before
     * setFormat on any of the planes.
     *
     * @param[in] method Type of interpolation method, one of enum
     *                   v4l2_interpolation_method
     *
     * @returns 0 for success, -1 for failure
     */
    int setInterpolationMethod(enum v4l2_interpolation_method method);

    /**
     * Set the flip method
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_VIDEO_CONVERT_FLIP_METHOD. Must be called before
     * setFormat on any of the planes.
     *
     * @param[in] method Type of flip method, one of enum v4l2_flip_method
     *
     * @returns 0 for success, -1 for failure
     */
    int setFlipMethod(enum v4l2_flip_method method);

    /**
     * Set the TNR(Temporal Noise Reduction) algorithm to use
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_VIDEO_CONVERT_TNR_ALGORITHM. Must be called before
     * setFormat on any of the planes.
     *
     * @param[in] algorithm Type of TNR algorithm to use, one of enum
     *                      v4l2_tnr_algorithm
     *
     * @returns 0 for success, -1 for failure
     */
    int setTnrAlgorithm(enum v4l2_tnr_algorithm algorithm);

    /**
     * Set the cropping rectangle for the converter
     *
     * Calls the VIDIOC_S_SELECTION internally on capture plane. Must be called
     * before setFormat on any of the planes.
     *
     * @param[in] left Horizontal offset of the rectangle, in pixels
     * @param[in] top  Verticaal offset of the rectangle, in pixels
     * @param[in] width Width of the rectangle, in pixels
     * @param[in] height Height of the rectangle, in pixels
     *
     * @returns 0 for success, -1 for failure
     */
    int setCropRect(uint32_t left, uint32_t top, uint32_t width,
            uint32_t height);

    /**
     * Wait till all the buffers queued on the output plane are converted and
     * dequeued from the capture plane. This is a blocking call.
     *
     * @param[in] max_wait_ms Maximum time to wait in milliseconds
     * @returns 0 for success, -1 for timeout
     */
    int waitForIdle(uint32_t max_wait_ms);

private:
    /**
     * Contructor used by #createVideoConverter
     */
    NvVideoConverter(const char *name, int flags);

    static const NvElementProfiler::ProfilerField valid_fields =
            NvElementProfiler::PROFILER_FIELD_TOTAL_UNITS |
            NvElementProfiler::PROFILER_FIELD_LATENCIES |
            NvElementProfiler::PROFILER_FIELD_FPS;
};

#endif
