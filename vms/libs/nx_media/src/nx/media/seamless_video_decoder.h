// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/media/media_fwd.h>
#include <nx/media/video_data_packet.h>

class QRhi;

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
class NX_MEDIA_API SeamlessVideoDecoder: public QObject
{
    Q_OBJECT

public:
    typedef std::function<QRect()> VideoGeometryAccessor;

public:
    using FrameHandler = std::function<void(VideoFramePtr frame)>;

    SeamlessVideoDecoder(FrameHandler hanlder, QRhi* rhi);

    virtual ~SeamlessVideoDecoder();

    /** Should be called before first decode(). */
    void setAllowHardwareAcceleration(bool value);

    /** Should be called before first decode(); needed by some decoders, e.g. hw-based. */
    void setVideoGeometryAccessor(VideoGeometryAccessor videoGeometryAccessor);

    /**
     * If true and the hardware decoder failed to decode the video, then the software decoder
     * will be used.
     */
    void setAllowSoftwareDecoderFallback(bool value);

    bool isSoftwareDecoderFallbackMode();

    /**
     * Decode a video packet. This is a sync function and it could take a lot of CPU. This call is
     * not thread-safe.
     * @param packet Compressed video data. If packet is null, then the function must flush
     * internal decoder buffer. If a decoded frame is ready, the FrameHandler callback will be
     * called. The handler may be called multiple times if multiple frames are ready or the decoder
     * is in flush mode.
     * @return True if packet is decoded without errors.
     */
    bool decode(const QnConstCompressedVideoDataPtr& packet);

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
    void pushFrame(VideoFramePtr frame);

private:
    QScopedPointer<SeamlessVideoDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(SeamlessVideoDecoder);
};

} // namespace media
} // namespace nx
