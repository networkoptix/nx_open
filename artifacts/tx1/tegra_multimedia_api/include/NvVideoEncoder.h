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
 * @file NvVideoEncoder.h
 *
 * @brief Helper class for V4L2 Video Encoder.
 */

#ifndef __NV_VIDEO_ENCODER_H__
#define __NV_VIDEO_ENCODER_H__

#include "NvV4l2Element.h"

/**
 * @brief Helper class for V4L2 Video Encoder.
 *
 * The video encoder device node is \b "/dev/nvhost-msenc". The category name
 * for encoder is \b "NVENC".
 *
 * Refer to @ref V4L2Enc for more information on the converter.
 */

class NvVideoEncoder:public NvV4l2Element
{
public:
    /**
     * Create a new V4L2 Video Encoder named \a name
     *
     * This function internally calls v4l2_open on the encoder dev node
     * \b "/dev/nvhost-msenc" and checks for \b V4L2_CAP_VIDEO_M2M_MPLANE
     * capability on the device. This method allows the caller to specify
     * additional flags with which the device should be opened.
     *
     * The device is opened in blocking mode which can be modified by passing
     * the @a O_NONBLOCK flag to this method.
     *
     * @returns Reference to the newly created encoder object else NULL in
     *          case of failure during initialization.
     */
    static NvVideoEncoder *createVideoEncoder(const char *name, int flags = 0);

     ~NvVideoEncoder();

    /**
     * Sets the format on the encoder output plane.
     *
     * Calls VIDIOC_S_FMT ioctl internally on the output plane.
     *
     * @pre Capture plane format (#setCapturePlaneFormat) must be set before calling this.
     *
     * @param[in] pixfmt One of the raw V4L2 pixel formats
     * @param[in] width Width of the input buffers in pixels
     * @param[in] height Height of the input buffers in pixels
     * @returns 0 for success, -1 for failure
     */
    int setOutputPlaneFormat(uint32_t pixfmt, uint32_t width, uint32_t height);
    /**
     * Sets the format on the encoder capture plane.
     *
     * Calls VIDIOC_S_FMT ioctl internally on the capture plane.
     *
     * @param[in] pixfmt One of the coded V4L2 pixel formats
     * @param[in] width Width of the input buffers in pixels
     * @param[in] height Height of the input buffers in pixels
     * @param[in] sizeimage Maximum size of the encoded buffers on the capture
     *                      plane in bytes
     * @returns 0 for success, -1 for failure
     */
    int setCapturePlaneFormat(uint32_t pixfmt, uint32_t width,
                              uint32_t height, uint32_t sizeimage);

    /**
     * Set the encode framerate
     *
     * Calls the VIDIOC_S_PARM ioctl on the encoder capture plane. Can b
     * called anytime after setFormat on both the planes.
     *
     * @param[in] framerate_num Numerator part of the framerate fraction
     * @param[in] framerate_den Denominator part of the framerate fraction
     *
     * @returns 0 for success, -1 for failure
     */
    int setFrameRate(uint32_t framerate_num, uint32_t framerate_den);

    /**
     * Set the encoder bitrate
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * V4L2_CID_MPEG_VIDEO_BITRATE. Can be called anytime after setFormat on
     * both the planes.
     *
     * @param[in] bitrate Bitrate of the encoded stream, in bits per second
     *
     * @returns 0 for success, -1 for failure
     */
    int setBitrate(uint32_t bitrate);

    /**
     * Set the encoder profile.
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * V4L2_CID_MPEG_VIDEO_H264_PROFILE or V4L2_CID_MPEG_VIDEO_H265_PROFILE
     * depending on the encoder type. Must be called after setFormat on both
     * the planes and before #requestBuffers on any of the planes.
     *
     * @param[in] profile Profile to be used for encoding
     *
     * @returns 0 for success, -1 for failure
     */
    int setProfile(uint32_t profile);

    /**
     * Set the encoder level.
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * V4L2_CID_MPEG_VIDEO_H264_LEVEL. Must be called after setFormat on both
     * the planes and before #requestBuffers on any of the planes.
     *
     * @param[in] level Level to be used for encoding, one of enum
     *                  v4l2_mpeg_video_h264_level
     *
     * @returns 0 for success, -1 for failure
     */
    int setLevel(enum v4l2_mpeg_video_h264_level level);

    /**
     * Set the encoder rate control mode
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEO_BITRATE_MODE. Must be called after setFormat on both
     * the planes and before #requestBuffers on any of the planes.
     *
     * @param[in] mode Type of rate control, one of enum
     *              v4l2_mpeg_video_bitrate_mode
     *
     * @returns 0 for success, -1 for failure
     */
    int setRateControlMode(enum v4l2_mpeg_video_bitrate_mode mode);

    /**
     * Set the encoder I-frame interval.
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * V4L2_CID_MPEG_VIDEO_GOP_SIZE. Must be called after setFormat on both
     * the planes and before #requestBuffers on any of the planes.
     *
     * @param[in] interval Interval between two I frames, in number of frames
     *
     * @returns 0 for success, -1 for failure
     */
    int setIFrameInterval(uint32_t interval);

    /**
     * Set the encoder IDR interval.
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEO_IDR_INTERVAL. Must be called after setFormat on both
     * the planes and before #requestBuffers on any of the planes.
     *
     * @param[in] interval Interval between two IDR frames, in number of frames
     *
     * @returns 0 for success, -1 for failure
     */
    int setIDRInterval(uint32_t interval);

    /**
     * Force an IDR frame
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE. Must be called after
     * setFormat on both the planes.
     *
     * @param[in] interval Interval between two IDR frames, in number of frames
     *
     * @returns 0 for success, -1 for failure
     */
    int forceIDR();

    /**
     * Set the encoder Temporal Tradeoff.
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEOENC_TEMPORAL_TRADEOFF_LEVEL. Must be called after
     * setFormat on both the planes and before #requestBuffers on any of the
     * planes.
     *
     * @param[in] level Temporal tradeoff level, one of
     *                  v4l2_enc_temporal_tradeoff_level_type
     * @returns 0 for success, -1 for failure
     */
    int setTemporalTradeoff(v4l2_enc_temporal_tradeoff_level_type level);

    /**
     * Set the encoder output slice length.
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEOENC_SLICE_LENGTH_PARAM. Must be called after setFormat on
     * both the planes and before #requestBuffers on any of the planes.
     *
     * @param[in] type Slice length type, one of enum v4l2_enc_slice_length_type
     * @param[in] length Length of the slice in bytes if type is
     *      V4L2_ENC_SLICE_LENGTH_TYPE_BITS, else in number of MBs if type is
     *      V4L2_ENC_SLICE_LENGTH_TYPE_MBLK
     * @returns 0 for success, -1 for failure
     */
    int setSliceLength(v4l2_enc_slice_length_type type, uint32_t length);

    /**
     * Set the Region of Interest parameters for the next buffer which will
     * be queued on output plane with index \a buffer_index
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEOENC_ROI_PARAMS. Must be called after
     * requesting buffer on both the planes.
     *
     * @param[in] buffer_index Index of output plane buffer on which the ROI
     *                         params should be applied.
     * @param[in] params Parameters to be applied on the frame, structure of
     *                   type #v4l2_enc_frame_ROI_params
     * @returns 0 for success, -1 for failure
     */
    int setROIParams(uint32_t buffer_index, v4l2_enc_frame_ROI_params & params);

    /**
     * Set the virtual buffer size of the encoder.
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEOENC_VIRTUALBUFFER_SIZE. Must be called after
     * setFormat on both the planes.
     *
     * @param[in] size Virtual buffer size, in bytes.
     * @returns 0 for success, -1 for failure
     */
    int setVirtualBufferSize(uint32_t size);

    /**
     * Set the number of reference frames of the encoder.
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEOENC_NUM_REFERENCE_FRAMES. Must be called after
     * setFormat on both the planes.
     *
     * @param[in] num_frames Number of reference frames.
     * @returns 0 for success, -1 for failure
     */
    int setNumReferenceFrames(uint32_t num_frames);

    /**
     * Set slice intrareferesh interval params.
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEOENC_SLICE_INTRAREFRESH_PARAM. Must be called after
     * setFormat on both the planes.
     *
     * @param[in] interval Slice intrarefresh interval, in number of slices.
     * @returns 0 for success, -1 for failure
     */
    int setSliceIntrarefresh(uint32_t interval);

    /**
     * Set the number of B frames two P frames.
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEOENC_NUM_BFRAMES. Must be called after
     * setFormat on both the planes.
     *
     * @param[in] num Number of B frames.
     * @returns 0 for success, -1 for failure
     */
    int setNumBFrames(uint32_t num);

    /**
     * Enabled/disable insert SPS PPS at every IDR.
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEOENC_INSERT_SPS_PPS_AT_IDR. Must be called after
     * setFormat on both the planes.
     *
     * @param[in] enabled Boolean value indicating wether to enable/disable
     *                    the control.
     * @returns 0 for success, -1 for failure
     */
    int setInsertSpsPpsAtIdrEnabled(bool enabled);

    /**
     * Enable video encoder output motion vector metadata reporting
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEOENC_METADATA_MV. Must be called after setFormat on
     * both the planes and before #requestBuffers on any of the planes.
     *
     * @returns 0 for success, -1 for failure
     */
    int enableMotionVectorReporting();

    /**
     * Get metadata for encoded capture plane buffer
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEOENC_METADATA. Must be called for a buffer which has
     * been dequeued from the capture plane. The returned metadata corresponds
     * to the last dequeued buffer with index @a buffer_index.
     *
     * @param[in] buffer_index Index of the capture plane buffer whose metadata
     *              is required
     * @param[in,out] Reference to the metadata structure
     *              v4l2_ctrl_videoenc_outputbuf_metadata to be filled
     *
     * @returns 0 for success, -1 for failure
     */
    int getMetadata(uint32_t buffer_index,
            v4l2_ctrl_videoenc_outputbuf_metadata &enc_metadata);

    /**
     * Get motion vector metadata for encoded capture plane buffer
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEOENC_METADATA_MV. Must be called for a buffer which has
     * been dequeued from the capture plane. The returned metadata corresponds
     * to the last dequeued buffer with index @a buffer_index.
     *
     * @param[in] buffer_index Index of the capture plane buffer whose metadata
     *              is required
     * @param[in,out] Reference to the metadata structure
     *              v4l2_ctrl_videoenc_outputbuf_metadata_MV to be filled
     *
     * @returns 0 for success, -1 for failure
     */
    int getMotionVectors(uint32_t buffer_index,
            v4l2_ctrl_videoenc_outputbuf_metadata_MV &enc_mv_metadata);

    /**
     * Set QP values for I/P/B frames
     *
     * Calls the VIDIOC_S_EXT_CTRLS ioctl internally with Control id
     * #V4L2_CID_MPEG_VIDEOENC_QP_RANGE. Must be called after
     * setFormat on both the planes.
     *
     * @param[in] MinQpI Minimum Qp Value for I frame
     * @param[in] MaxQpI Minimum Qp Value for I frame
     * @param[in] MinQpP Minimum Qp Value for P frame
     * @param[in] MaxQpP Minimum Qp Value for P frame
     * @param[in] MinQpB Minimum Qp Value for B frame
     * @param[in] MaxQpB Minimum Qp Value for B frame
     * @returns 0 for success, -1 for failure
     */
    int setQpRange(uint32_t MinQpI, uint32_t MaxQpI, uint32_t MinQpP,
            uint32_t MaxQpP, uint32_t MinQpB, uint32_t MaxQpB);


private:
    /**
     * Contructor used by #createVideoEncoder
     */
    NvVideoEncoder(const char *name, int flags);

    static const NvElementProfiler::ProfilerField valid_fields =
            NvElementProfiler::PROFILER_FIELD_TOTAL_UNITS |
            NvElementProfiler::PROFILER_FIELD_LATENCIES |
            NvElementProfiler::PROFILER_FIELD_FPS;
};

#endif
