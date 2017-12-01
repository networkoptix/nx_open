#pragma once

#include <functional>

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>
#include <QtGui/QOpenGLContext>

#include "abstract_resource_allocator.h"
#include <nx/streaming/video_data_packet.h>

#include "media_fwd.h"

namespace nx {
namespace media {

/**
 * Interface for video decoder implementation. Each derived class should provide a constructor with
 * the following signature:
 * <pre> ...VideoDecoder(const ResourceAllocatorPtr& allocator, const QSize& resolution); </pre>
 *
 */
class AbstractVideoDecoder: public QObject
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
        const AVCodecID /*codec*/, const QSize& /*resolution*/, bool /*allowOverlay*/)
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
     * Decode a video frame. This is a sync function and it could take a lot of CPU.
     * It's guarantee all source frames have same codec context.
     *
     * @param compressedVideoData Compressed single frame data. If null, this function must flush
     *     internal decoder buffer. If no more frames are left in the buffer, return zero value as
     *     a result, and set outDecodedFrame to null.
     *     ATTENTION: compressedVideoData->data() is guaranteed to be non-null, have dataSize() > 0
     *     and have additional QN_BYTE_ARRAY_PADDING bytes allocated after frame->dataSize(), which
     *     is needed by e.g. ffmpeg decoders and can be zeroed by this function.
     *
     * @param outDecodedFrame Decoded video data. If decoder still fills its internal buffer, then
     *     outDecodedFrame should be set to null, and the function should return 0.
     *
     * @return Decoded frame number (value >= 0), if the frame is decoded without errors. Return a
     *     negative value on decoding error. For null input data, return positive value while the
     *     decoder is flushing its internal buffer (outDecodedFrame is not set to null).
     */
    virtual int decode(
        const QnConstCompressedVideoDataPtr& compressedVideoData,
        QVideoFramePtr* outDecodedFrame) = 0;

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

    /**
     * @return Frame size in pixels.
     */
    static QSize mediaSizeFromRawData(const QnConstCompressedVideoDataPtr& frame);

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
