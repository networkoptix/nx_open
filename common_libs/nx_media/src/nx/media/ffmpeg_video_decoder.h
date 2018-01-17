#pragma once

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

#include "abstract_video_decoder.h"

namespace nx {
namespace media {

class FfmpegVideoDecoderPrivate;

/**
 * Implements ffmpeg video decoder.
 */
class FfmpegVideoDecoder: public AbstractVideoDecoder
{
public:
    /** @param maxResolution Limits applicability of the decoder. If empty, there is no limit.
     * Map key with value 0 means default resolution limit otherwise limit for specified AV codec.
     */
    static void setMaxResolutions(const QMap<int, QSize>& maxResolutions);

    FfmpegVideoDecoder(const ResourceAllocatorPtr& allocator, const QSize& resolution);
    virtual ~FfmpegVideoDecoder();

    static bool isCompatible(
        const AVCodecID codec, const QSize& resolution, bool allowOverlay);

    static QSize maxResolution(const AVCodecID codec);

    virtual int decode(
        const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result = nullptr) override;

    virtual double getSampleAspectRatio() const override;

    virtual Capabilities capabilities() const override;
private:
    static QMap<int, QSize> s_maxResolutions;

    QScopedPointer<FfmpegVideoDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(FfmpegVideoDecoder);
};

} // namespace media
} // namespace nx
