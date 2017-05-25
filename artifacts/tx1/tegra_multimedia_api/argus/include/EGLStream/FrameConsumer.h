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

#ifndef _EGLSTREAM_FRAME_CONSUMER_H
#define _EGLSTREAM_FRAME_CONSUMER_H

#include "Frame.h"

namespace EGLStream
{

/**
 * A FrameConsumer object acts as a consumer endpoint to an OutputStream or
 * EGLStream, provided during creation, and exposes interfaces to return
 * Frame objects that provide various image reading interfaces.
 *
 * Destroying a Consumer will implicitly disconnect the stream and release any
 * pending or acquired frames, invalidating any currently acquired Frame objects.
 */
class FrameConsumer : public Argus::InterfaceProvider, public Argus::Destructable
{
public:
    /**
     * Creates a new FrameConsumer to read frames from an Argus OutputStream.
     *
     * @param[in] outputStream The output stream to read from.
     * @param[in] maxFrames The maximum number of frames that can be acquired
     *                      by this consumer at any point in time. Default is 1.
     * @param[out] status An optional pointer to return an error status code.
     *
     * @returns A new FrameConsumer object, or NULL on error.
     */
    static FrameConsumer* create(Argus::OutputStream* outputStream,
                                 uint32_t maxFrames = 1,
                                 Argus::Status* status = NULL);

    /**
     * Creates a new FrameConsumer to read frames from an EGLStream.
     *
     * @param[in] eglDisplay The EGLDisplay the stream belongs to.
     * @param[in] eglDisplay The EGLStream to connect to.
     * @param[in] maxFrames The maximum number of frames that can be acquired
     *                      by this consumer at any point in time. Default is 1.
     * @param[out] status An optional pointer to return an error status code.
     *
     * @returns A new FrameConsumer object, or NULL on error.
     */
    static FrameConsumer* create(EGLDisplay eglDisplay,
                                 EGLStreamKHR eglStream,
                                 uint32_t maxFrames = 1,
                                 Argus::Status* status = NULL);
protected:
    ~FrameConsumer() {}
};

/**
 * @class IFrameConsumer
 *
 * Exposes the methods used to acquire Frames from a FrameConsumer.
 */
DEFINE_UUID(Argus::InterfaceID, IID_FRAME_CONSUMER, b94a7bd1,c3c8,11e5,a837,08,00,20,0c,9a,66);
class IFrameConsumer : public Argus::Interface
{
public:
    static const Argus::InterfaceID& id() { return IID_FRAME_CONSUMER; }

    /**
     * Acquires a new frame from the FrameConsumer, returning a Frame object. If the maximum
     * number of frames are already acquired from the stream, an error will be returned
     * immediately. If NULL is returned and the status code is STATUS_DISCONNECTED, the
     * producer has disconnected from the stream and no more frames can be acquired.
     *
     * @param[in] timeout The timeout (in nanoseconds) to wait for a frame if one isn't available.
     * @param[out] status An optional pointer to return an error status code.
     *
     * @returns A pointer to the frame acquired from the stream, or NULL on error.
     */
    virtual Frame* acquireFrame(uint64_t timeout = Argus::TIMEOUT_INFINITE,
                                Argus::Status* status = NULL) = 0;

protected:
    ~IFrameConsumer() {}
};

} // namespace EGLStream

#endif // _EGLSTREAM_FRAME_CONSUMER_H
