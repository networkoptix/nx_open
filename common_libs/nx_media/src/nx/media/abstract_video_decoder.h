#pragma once

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
class AbstractVideoDecoder
:
    public QObject
{
    Q_OBJECT
public:
    virtual ~AbstractVideoDecoder() = default;

    /**
     * This function should be overridden despite static keyword. Otherwise it is a compile error.
     * @param context Codec context.
     * @return True if the decoder is compatible with the provided parameters.
     */
    static bool isCompatible(const CodecID codec, const QSize& resolution)
    {
        QN_UNUSED(codec, resolution);
        return false;
    }

    /**
     * Decode a video frame. This is a sync function and it could take a lot of CPU.
     * It's guarantee all source frames have same codec context.
     *
     * @param compressedVideoData Compressed single frame data. If null, this function must flush
     * internal decoder buffer. If no more frames are left in the buffer, return zero value as a
     * result, and set outDecodedFrame to null.
     * ATTENTION: compressedVideoData->data() is guaranteed to be non-null, have dataSize() > 0 and
     * have additional QN_BYTE_ARRAY_PADDING bytes allocated after frame->dataSize(), which is
     * needed by e.g. ffmpeg decoders and can be zeroed by this function.
     *
     * @param outDecodedFrame Decoded video data. If decoder still fills its internal buffer, then
     * outDecodedFrame should be set to null, and the function should return 0.
     *
     * @return Decoded frame number (value >= 0), if the frame is decoded without errors. Return a
     * negative value on decoding error. For null input data, return positive value while the
     * decoder is flushing its internal buffer (outDecodedFrame is not set to null).
     */
    virtual int decode(
        const QnConstCompressedVideoDataPtr& compressedVideoData, QVideoFramePtr* outDecodedFrame) = 0;
};

} // namespace media
} // namespace nx
