#pragma once
#if defined(Q_OS_ANDROID)

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

#include "abstract_video_decoder.h"

namespace nx {
namespace media {

class AndroidVideoDecoderPrivate;

/**
 * Implements hardware android video decoder.
 */
class AndroidVideoDecoder
:
    public AbstractVideoDecoder
{
public:
    AndroidVideoDecoder(const ResourceAllocatorPtr& allocator, const QSize& resolution);
    virtual ~AndroidVideoDecoder();

    static bool isCompatible(
        const AVCodecID codec, const QSize& resolution, bool allowOverlay);

    static QSize maxResolution(const AVCodecID codec);

    virtual int decode(const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result = nullptr) override;

    virtual Capabilities capabilities() const override;
private:
    std::shared_ptr<AndroidVideoDecoderPrivate> d;
    // TODO: Fix: Q_DECLARE_PRIVATE requires QSharedPtr and d_ptr field.
    Q_DECLARE_PRIVATE(AndroidVideoDecoder);
};

} // namespace media
} // namespace nx

#endif // Q_OS_ANDROID
