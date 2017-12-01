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

#ifndef _ARGUS_STREAM_H
#define _ARGUS_STREAM_H

namespace Argus
{

DEFINE_NAMED_UUID_CLASS(StreamMode);
DEFINE_UUID(StreamMode, STREAM_MODE_MAILBOX, 33661d40,3ee2,11e6,bdf4,08,00,20,0c,9a,66);
DEFINE_UUID(StreamMode, STREAM_MODE_FIFO,    33661d41,3ee2,11e6,bdf4,08,00,20,0c,9a,66);

/**
 * Input streams are created and owned by CaptureSessions, and they maintain
 * a connection with an EGLStream in order to acquire frames as an EGLStream consumer.
 */
class InputStream : public InterfaceProvider, public Destructable
{
protected:
    ~InputStream() {}
};

/**
 * Output streams are created and owned by CaptureSessions, and they maintain
 * a connection with an EGLStream in order to present frames as an EGLStream producer.
 */
class OutputStream : public InterfaceProvider, public Destructable
{
protected:
    ~OutputStream() {}
};

/**
 * Settings for OutputStream creation are exposed by the OutputStreamSettings class.
 * These are created by CaptureSessions and used for calls to ICaptureSession::createOutputStream.
 */
class OutputStreamSettings : public InterfaceProvider, public Destructable
{
protected:
    ~OutputStreamSettings() {}
};

/**
 * @class IStream
 *
 * Interface that exposes the properties common to all Stream objects.
 */
DEFINE_UUID(InterfaceID, IID_STREAM, 8f50dade,cc26,4ec6,9d2e,d9,d0,19,2a,ef,06);

class IStream : public Interface
{
public:
    static const InterfaceID& id() { return IID_STREAM; }

    /**
     * Waits until both the producer and consumer endpoints of the stream are connected.
     *
     * @param[in] timeout The timeout in nanoseconds.
     *
     * @returns success/status of this call.
     */
    virtual Status waitUntilConnected(uint64_t timeout = TIMEOUT_INFINITE) const = 0;

    /**
     * Disconnects the stream from the underlying EGLStream.
     */
    virtual void disconnect() = 0;

    /**
     * Returns the format of the stream.
     */
    virtual PixelFormat getPixelFormat() const = 0;

    /**
     * Returns the image resolution of the stream, in pixels.
     */
    virtual Size getResolution() const = 0;

    /**
     * Returns the EGLDisplay the stream's EGLStream belongs to.
     */
    virtual EGLDisplay getEGLDisplay() const = 0;

    /**
     * Returns the EGLStream backing the stream.
     */
    virtual EGLStreamKHR getEGLStream() const = 0;

protected:
    ~IStream() {}
};

/**
 * @class IOutputStreamSettings
 *
 * Interface that exposes the settings used for OutputStream creation.
 */
DEFINE_UUID(InterfaceID, IID_OUTPUT_STREAM_SETTINGS, 52f2b830,3d52,11e6,bdf4,08,00,20,0c,9a,66);

class IOutputStreamSettings : public Interface
{
public:
    static const InterfaceID& id() { return IID_OUTPUT_STREAM_SETTINGS; }

    /**
     * Set the format of the stream.
     *   Default value: PIXEL_FMT_UNKNOWN
     */
    virtual Status setPixelFormat(const PixelFormat& format) = 0;
    virtual PixelFormat getPixelFormat() const = 0;

    /**
     * Set the resolution of the stream.
     *   Default value: (0, 0)
     */
    virtual Status setResolution(const Size& resolution) = 0;
    virtual Size getResolution() const = 0;

    /**
     * Set the camera device to use as the source for this stream.
     *   Default value: First available device in the session.
     */
    virtual Status setCameraDevice(CameraDevice* device) = 0;
    virtual CameraDevice* getCameraDevice() const = 0;

    /**
     * Set the EGLDisplay the created stream must belong to.
     *   Default value: EGL_NO_DISPLAY - stream is display-agnostic.
     */
    virtual Status setEGLDisplay(EGLDisplay eglDisplay) = 0;
    virtual EGLDisplay getEGLDisplay() const = 0;

    /**
     * Sets the mode of the OutputStream. Available options are:
     *
     *   MAILBOX:
     *     In this mode, only the newest frame is made available to the consumer. When Argus
     *     completes a frame it empties the mailbox and inserts the new frame into the mailbox.
     *     The consumer then retrieves the frame from the mailbox and processes it; when
     *     finished, the frame is either placed back into the mailbox (if the mailbox is empty)
     *     or discarded (if the mailbox is not empty). This mode implies 2 things:
     *
     *       - If the consumer consumes frames slower than Argus produces frames, then some
     *         frames may be lost (never seen by the consumer).
     *
     *       - If the consumer consumes frames faster than Argus produces frames, then the
     *         consumer may see some frames more than once.
     *
     *   FIFO:
     *     When using this mode, every producer frame is made available to the consumer through
     *     the use of a fifo queue for the frames. When using this mode, the fifo queue length
     *     must be specified using setFifoLength. When Argus completes a frame it inserts it to
     *     the head of the fifo queue. If the fifo is full (already contains the number of frames
     *     equal to the fifo queue length), Argus will stall until the fifo is no longer
     *     full. The consumer consumes frames from the tail of the queue; however, if the
     *     consumer releases a frame while the queue is empty, the frame is set aside and will
     *     be returned again the next time the consumer requests a frame if another new frame
     *     has not been inserted into the fifo queue before then. Once a new frame is inserted
     *     into the fifo queue, any previously released frame will be permanently discarded.
     *     This mode implies:
     *
     *       - Frames are never discarded until the consumer has processed them.
     *
     *       - If the consumer consumes frames slower than Argus produces them, Argus will stall.
     *
     *       - If the consumer consumes frames faster than Argus produces them, then the
     *         consumer may see some frames more than once.
     *
     *   Default value: STREAM_MODE_MAILBOX
     */
    virtual Status setMode(const StreamMode& mode) = 0;
    virtual StreamMode getMode() const = 0;

    /**
     * Sets the FIFO queue length of the stream. This value is only used if the stream is using
     * the FIFO mode (@see OutputStreamSettings::setMode). Value must be > 0.
     *   Default value: 1
     */
    virtual Status setFifoLength(uint32_t fifoLength) = 0;
    virtual uint32_t getFifoLength() const = 0;

protected:
    ~IOutputStreamSettings() {}
};

} // namespace Argus

#endif // _ARGUS_STREAM_H
