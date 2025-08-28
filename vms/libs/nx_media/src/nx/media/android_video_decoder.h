// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once
#include <QtCore/QtGlobal>
#if defined(Q_OS_ANDROID)

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/media/video_data_packet.h>

#include "abstract_video_decoder.h"

namespace nx::media {

/**
 * Implements hardware android video decoder.
 */
class AndroidVideoDecoder
:
    public AbstractVideoDecoder
{
public:
    AndroidVideoDecoder(const RenderContextSynchronizerPtr& synchronizer, const QSize& resolution);
    virtual ~AndroidVideoDecoder();

    static bool isCompatible(
        const AVCodecID codec,
        const QSize& resolution,
        bool allowOverlay,
        bool allowHardwareAcceleration);

    static QSize maxResolution(const AVCodecID codec);

    virtual bool sendPacket(const QnConstCompressedVideoDataPtr& packet) override;
    virtual bool receiveFrame(VideoFramePtr* decodedFrame) override;
    virtual int currentFrameNumber() const override;

    virtual Capabilities capabilities() const override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::media

#endif // Q_OS_ANDROID
