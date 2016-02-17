#pragma once

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#if defined(Q_OS_ANDROID)

#include <nx/streaming/video_data_packet.h>

#include "abstract_video_decoder.h"

namespace nx {
namespace media {

/*
* This class implements hardware android video decoder
*/
class AndroidDecoderPrivate;
class AndroidDecoder : public AbstractVideoDecoder
{
public:
    AndroidDecoder();
    virtual ~AndroidDecoder();

    static bool isCompatible(const CodecID codec, const QSize& resolution);
    virtual int decode(const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result = nullptr) override;
    virtual void setAllocator(AbstractResourceAllocator* allocator) override;
private:
    QScopedPointer<AndroidDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(AndroidDecoder);
};

}
}

#endif // #defined(Q_OS_ANDROID)
