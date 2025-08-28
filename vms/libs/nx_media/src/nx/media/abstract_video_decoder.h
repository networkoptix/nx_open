// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QObject>
#include <QtGui/QOpenGLContext>
#include <QtMultimedia/QVideoFrame>

#include <nx/media/video_data_packet.h>

#include "abstract_render_context_synchronizer.h"
#include "media_fwd.h"

namespace nx {
namespace media {

/**
 * Interface for video decoder implementation. Each derived class should provide a constructor with
 * the following signature:
 * <pre> ...VideoDecoder(const RenderContextSynchronizerPtr& synchronizer, const QSize& resolution); </pre>
 *
 */
class NX_MEDIA_API AbstractVideoDecoder: public QObject
{
    Q_OBJECT

public:
    typedef std::function<QRect()> VideoGeometryAccessor;

    enum class Capability
    {
        noCapability = 0,
        hardwareAccelerated = 1 << 0,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability);

public:
    virtual ~AbstractVideoDecoder() = default;

    /**
     * @return video decoder capabilities
     */
    virtual Capabilities capabilities() const = 0;

    /**
     * Used from a template; should be overridden despite being static.
     * @return True if the decoder is compatible with the provided parameters.
     */
    static bool isCompatible(
        const AVCodecID /*codec*/,
        const QSize& /*resolution*/,
        bool /*allowOverlay*/,
        bool /*allowHardwareAcceleration*/)
    {
        return false;
    }

    /**
     * Used from a template; should be overridden despite being static.
     * ATTENTION: The definition of maximum for resolution is left somewhat fuzzy: complete
     * implementation would probably require to define the maximum as a set of resolutions with
     * either maximum width or maximum height (similar to how Android does it).
     * @return Max supported resolution for the specified codec, or Invalid if there is no limit or
     * the codec is not supported.
     */
    static QSize maxResolution(const AVCodecID /*codec*/)
    {
        return QSize();
    }

    /**
     * Decode a video packet. This is a sync function and it could take a lot of CPU. This call is
     * not thread-safe.
     * @param packet Compressed video data. If packet is null, then the function must flush
     * internal decoder buffer.
     * It is possible that multiple frames are ready if the decoder is in flush mode.
     * @return True if packet is decoded without errors.
     */
    virtual bool sendPacket(const QnConstCompressedVideoDataPtr& packet) = 0;

    /**
     * Call receiveFrame in a loop after each sendPacket until it returns nullptr in decodedFrame.
     * @param decodedFrame The decoded video frame, or nullptr if there are no ready frames.
     * @return True if packet is decoded without errors.
     */
    virtual bool receiveFrame(VideoFramePtr* decodedFrame) = 0;

    /**
     * @return Current frame number in range [0..INT_MAX]. This number of the last decoded frame.
     */
    virtual int currentFrameNumber() const = 0;

    /**
     * Should be called before other methods. Allows to obtain video window coords for some
     * decoders, e.g. hw-based.
     */
    void setVideoGeometryAccessor(VideoGeometryAccessor videoGeometryAccessor)
    {
        m_videoGeometryAccessor = videoGeometryAccessor;
    }

    /**
     * @return Square(pixel) aspect ratio (sar) for the last decoded frame,
     * or 1.0 if context is not initialized yet.
     */
    virtual double getSampleAspectRatio() const { return 1.0; }

protected:
    /**
     * Retrieve current video window position and size via VideoGeometryAccessor.
     * @return Null QRect if VideoGeometryAccessor has not been set.
     */
    QRect getVideoGeometry() const
    {
        if (!m_videoGeometryAccessor)
            return QRect();
        return m_videoGeometryAccessor();
    }

private:
    VideoGeometryAccessor m_videoGeometryAccessor;
};

} // namespace media
} // namespace nx
