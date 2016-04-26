#pragma once

#if defined(Q_OS_ANDROID)

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

#include "abstract_video_decoder.h"

namespace nx {
namespace media {

/*
* This class implements hardware android video decoder
*/
class AndroidVideoDecoderPrivate;
class AndroidVideoDecoder : public AbstractVideoDecoder
{
public:
    AndroidVideoDecoder();
    virtual ~AndroidVideoDecoder();

    static bool isCompatible(const CodecID codec, const QSize& resolution);
    virtual int decode(const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result = nullptr) override;
    virtual void setAllocator(AbstractResourceAllocator* allocator) override;
private:
    std::shared_ptr<AndroidVideoDecoderPrivate> d;
    // TODO: Fix: Q_DECLARE_PRIVATE requires QSharedPtr and d_ptr field.
    Q_DECLARE_PRIVATE(AndroidVideoDecoder);
};

}
}

#endif // Q_OS_ANDROID
