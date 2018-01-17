#pragma once
#if defined (Q_OS_IOS)

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

#include "abstract_video_decoder.h"

namespace nx {
namespace media {

class IOSVideoDecoderPrivate;

/**
 * Implements ffmpeg video decoder.
 */
class IOSVideoDecoder: public AbstractVideoDecoder
{
public:
    IOSVideoDecoder(const ResourceAllocatorPtr& allocator, const QSize& resolution);
    virtual ~IOSVideoDecoder();

    static bool isCompatible(
        const AVCodecID codec,
        const QSize& resolution,
        bool allowOverlay);

    static QSize maxResolution(const AVCodecID codec);

    virtual int decode(
        const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result = nullptr) override;

    virtual Capabilities capabilities() const override;
private:
    void ffmpegToQtVideoFrame(QVideoFramePtr* result);

private:
    QScopedPointer<IOSVideoDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(IOSVideoDecoder);
};

} // namespace media
} // namespace nx

#endif // Q_OS_IOS
