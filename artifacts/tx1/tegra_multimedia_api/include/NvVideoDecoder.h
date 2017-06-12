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
 * @file NvVideoDecoder.h
 *
 * @brief Helper class for V4L2 Video Decoder.
 */

#ifndef __NV_VIDEO_DECODER_H__
#define __NV_VIDEO_DECODER_H__

#include "NvV4l2Element.h"

/**
 * @brief Helper class for V4L2 Video Decoder.
 *
 * The video decoder device node is \b "/dev/nvhost-nvdec". The category name
 * for encoder is \b "NVDEC".
 *
 * Refer to @ref V4L2Dec for more information on the converter.
 */
class NvVideoDecoder:public NvV4l2Element
{
public:
    /**
     * Create a new V4L2 Video Decoder named \a name.
     *
     * This function internally calls v4l2_open on the decoder dev node
     * \b "/dev/nvhost-nvdec" and checks for \b V4L2_CAP_VIDEO_M2M_MPLANE
     * capability on the device. This method allows the caller to specify
     * additional flags with which the device should be opened.
     *
     * The device is opened in blocking mode which can be modified by passing
     * the @a O_NONBLOCK flag to this method.
     *
     * @returns Reference to the newly created decoder object else \a NULL in
     *          case of failure during initialization.
     */
    static NvVideoDecoder *createVideoDecoder(const char *name, int flags = 0);

     ~NvVideoDecoder();
    /**
     * Sets the format on the decoder output plane.
     *
     * Calls VIDIOC_S_FMT ioctl internally on the capture plane.
     *
     * @param[in] pixfmt One of the raw V4L2 pixel formats
     * @param[in] width Width of the output buffers in pixels
     * @param[in] height Height of the output buffers in pixels
     * @returns 0 for success, -1 for failure.
     */
    int setCapturePlaneFormat(uint32_t pixfmt, uint32_t width, uint32_t height);
    /**
     * Sets the format on the decoder output plane.
     *
     * Calls VIDIOC_S_FMT ioctl internally on the output plane.
     *
     * @param[in] pixfmt One of the coded V4L2 pixel formats
     * @param[in] sizeimage Maximum size of the buffers on the output plane
                            containing encoded data in bytes
     * @returns 0 for success, -1 for failure.
     */
    int setOutputPlaneFormat(uint32_t pixfmt, uint32_t sizeimage);

    /**
     * Inform decoder input buffers may not contain complete frames
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEO_DISABLE_COMPLETE_FRAME_INPUT. Must be called before
     * setFormat on any of the planes.
     *
     * @returns 0 for success, -1 for failure
     */
    int disableCompleteFrameInputBuffer();

    /**
     * Disable display picture buffer
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEO_DISABLE_DPB. Must be called after setFormat on both
     * the planes and before #requestBuffers on any of the planes.
     *
     * @returns 0 for success, -1 for failure
     */
    int disableDPB();

    /**
     * Get the minimum number of buffers to be requested on decoder capture plane
     *
     * Calls the VIDIOC_G_CTRL ioctl internally with Control id
     * V4L2_CID_MIN_BUFFERS_FOR_CAPTURE. Is valid after the first
     * #V4L2_RESOLUTION_CHANGE_EVENT and may change after each subsequent
     * event.
     *
     * @param[out] num Pointer to integer to return the number of buffers
     *
     * @returns 0 for success, -1 for failure
     */
    int getMinimumCapturePlaneBuffers(int & num);

    /**
     * Set skip-frames parameter of the decoder
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEO_SKIP_FRAMES. Must be called after setFormat on both
     * the planes and before #requestBuffers on any of the planes.
     *
     * @param[in] skip_frames Type of frames to skip decoding, one of
     *                        enum v4l2_skip_frames_type
     *
     * @returns 0 for success, -1 for failure
     */
    int setSkipFrames(enum v4l2_skip_frames_type skip_frames);

    /**
     * Enable video decoder output metadata reporting
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEO_ERROR_REPORTING. Must be called after setFormat on
     * both the planes and before #requestBuffers on any of the planes.
     *
     * @returns 0 for success, -1 for failure
     */
    int enableMetadataReporting();

    /**
     * Get metadata for decoded capture plane buffer
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEODEC_METADATA. Must be called for a buffer which has
     * been dequeued from the capture plane. The returned metadata corresponds
     * to the last dequeued buffer with index @a buffer_index.
     *
     * @param[in] buffer_index Index of the capture plane buffer whose metadata
     *              is required
     * @param[in,out] Reference to the metadata structure
     *              v4l2_ctrl_videodec_outputbuf_metadata to be filled
     *
     * @returns 0 for success, -1 for failure
     */
    int getMetadata(uint32_t buffer_index,
        v4l2_ctrl_videodec_outputbuf_metadata &metadata);

private:
    /**
     * Contructor used by #createVideoDecoder.
     */
    NvVideoDecoder(const char *name, int flags);

    static const NvElementProfiler::ProfilerField valid_fields =
            NvElementProfiler::PROFILER_FIELD_TOTAL_UNITS |
            NvElementProfiler::PROFILER_FIELD_FPS;
};

#endif
