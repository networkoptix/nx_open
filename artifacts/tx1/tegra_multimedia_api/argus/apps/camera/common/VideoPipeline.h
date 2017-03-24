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

#ifndef VIDEO_PIPELINE_H
#define VIDEO_PIPELINE_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifdef GST_SUPPORTED
#include <gst/gst.h>
#endif

namespace ArgusSamples
{

/**
 * Record a video from a EGL stream
 */
class VideoPipeline
{
public:
    VideoPipeline();
    ~VideoPipeline();

    /**
     * Supported video formats
     */
    typedef enum
    {
        VIDEO_FORMAT_H263,
        VIDEO_FORMAT_H264,
        VIDEO_FORMAT_H265,
        VIDEO_FORMAT_VP8,
        VIDEO_FORMAT_MPEG4
    } VideoFormat;

    /**
     * Supported video file types
     */
    typedef enum
    {
        VIDEO_FILE_TYPE_MP4,
        VIDEO_FILE_TYPE_3GP,
        VIDEO_FILE_TYPE_AVI,
        VIDEO_FILE_TYPE_MKV,
        VIDEO_FILE_TYPE_H265
    } VideoFileType;

    /**
     * Destroy the video pipeline
     */
    bool destroy();

    /**
     * Setup the video pipeline for recording
     *
     * @param[in] videoStream   EGL stream to record from
     * @param[in] width         Width of the recorded video
     * @param[in] height        Height of the recorded video
     * @param[in] frameRate     Frame rate
     * @param[in] fileName      File name
     * @param[in] videoFormat   Video format
     * @param[in] videoFileType Video file type
     * @param[in] bitRate       Bitrate, if 0 the bitrate will be selected depending on the
     *                          resolution
     */
    bool setupForRecording(EGLStreamKHR videoStream, uint32_t width, uint32_t height,
            float frameRate, const char *fileName,
            VideoFormat videoFormat = VIDEO_FORMAT_H264,
            VideoFileType videoFileType = VIDEO_FILE_TYPE_MP4, uint32_t bitRate = 0);

    /**
     * Setup the video pipeline for playback
     *
     * @param[out] videoStream  EGL stream
     * @param[in] fileName      File name
     */
    bool setupForPlayback(EGLStreamKHR *videoStream, const char *fileName);

    /**
     * Start recording/playback
     */
    bool start();

    /**
     * Pause recording/playback
     */
    bool pause();

    /**
     * Toggle recording/playback
     */
    bool toggle();

    /**
     * Rewind (playback only)
     */
    bool rewind();

    /**
     * Stop recording/playback
     */
    bool stop();

    /**
     * Get the file extension for a video file type.
     */
    static const char* getFileExtension(VideoFileType fileType);

    /**
     * Get the aspect ratio of the video. The video has to be in paused or playing state.
     *
     * @param aspectRatio [out]
     */
    bool getAspectRatio(float *aspectRatio) const;

private:
#ifdef GST_SUPPORTED
    GstState m_state;

    GstElement *m_pipeline;
#endif
};

}; // namespace ArgusSamples

#endif // VIDEO_PIPELINE_H
