#pragma once

#include <QtCore/QObject>

#include <nx/streaming/video_data_packet.h>

#include "media_fwd.h"

namespace nx {
namespace media {

class AbstractVideoDecoder;
class SeamlessVideoDecoderPrivate;

/**
 * This class encapsulates common logic related to any video decoder. It guarantees seamless
 * decoding in case a compressed frame has changed resolution or codecId. VideoDecoder uses
 * VideoDecoderFactory to instantiate compatible VideoDecoder to decode next frame if
 * video parameters have changed.
 */
class SeamlessVideoDecoder: public QObject
{
    Q_OBJECT

public:
    typedef std::function<QRect()> VideoGeometryAccessor;

public:
    SeamlessVideoDecoder();

    virtual ~SeamlessVideoDecoder();

    /** Should be called before first decode(). */
    void setAllowOverlay(bool value);

    /** Should be called before first decode(); needed by some decoders, e.g. hw-based. */
    void setVideoGeometryAccessor(VideoGeometryAccessor videoGeometryAccessor);

    /**
     * Decode a video frame. This is a sync function and it could take a lot of CPU. This call is
     * not thread-safe.
     * @param frame Compressed video data. If frame is null, then the function must flush internal
     * decoder buffer. If no more frames in the buffer are left, function must return true as a
     * result and null shared pointer in the 'result' parameter.
     * @param result Decoded video data. If the decoder still fills internal buffer, then result
     * can be empty but the function returns true.
     * @return True if frame is decoded without errors. For null input data, return true while
     * flushing internal buffer (result is not null)
     */
    bool decode(const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result = nullptr);

    /**
     * @return Current frame number in range [0..INT_MAX]. This number will be used for the next
     * frame on a decode() call.
     */
    int currentFrameNumber() const;

    /** Can be empty if not available. */
    QSize currentResolution() const;

    /** Can be AV_CODEC_ID_NONE if not available. */
    AVCodecID currentCodec() const;

    AbstractVideoDecoder* currentDecoder() const;

    void pleaseStop();

private:
    QScopedPointer<SeamlessVideoDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(SeamlessVideoDecoder);
};

} // namespace media
} // namespace nx
