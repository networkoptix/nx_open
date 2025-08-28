// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/media/video_data_packet.h>

#include "abstract_video_decoder.h"

namespace nx {
namespace media {

class FfmpegVideoDecoderPrivate;

/**
 * Implements ffmpeg video decoder.
 */
class NX_MEDIA_API FfmpegVideoDecoder: public AbstractVideoDecoder
{
public:
    /** @param maxResolution Limits applicability of the decoder. If empty, there is no limit.
     * Map key with value 0 means default resolution limit otherwise limit for specified AV codec.
     */
    static void setMaxResolutions(const QMap<int, QSize>& maxResolutions);

    FfmpegVideoDecoder(const RenderContextSynchronizerPtr& synchronizer, const QSize& resolution);
    virtual ~FfmpegVideoDecoder();

    static bool isCompatible(
        const AVCodecID codec,
        const QSize& resolution,
        bool allowOverlay,
        bool allowHardwareAcceleration);

    static QSize maxResolution(const AVCodecID codec);

    // Create non-owning VideoFrame from AVFrame.
    static VideoFrame* fromAVFrame(const AVFrame* frame);

    virtual bool sendPacket(const QnConstCompressedVideoDataPtr& packet) override;
    virtual bool receiveFrame(VideoFramePtr* decodedFrame) override;
    virtual int currentFrameNumber() const override;

    virtual double getSampleAspectRatio() const override;

    virtual Capabilities capabilities() const override;
private:
    static QMap<int, QSize> s_maxResolutions;

    QScopedPointer<FfmpegVideoDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(FfmpegVideoDecoder);
};

} // namespace media
} // namespace nx
