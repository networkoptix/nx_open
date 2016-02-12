#pragma once

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

#include "abstract_video_decoder.h"

namespace nx {
namespace media {

/*
* This class implements software JPEG video decoder either via libJpeg (if exists) or QT
*/
class JpegDecoderPrivate;
class JpegDecoder : public AbstractVideoDecoder
{
public:
    JpegDecoder();

    static bool isCompatible(const CodecID codec, const QSize& resolution);
    virtual int decode(const QnConstCompressedVideoDataPtr& frame, QnVideoFramePtr* result = nullptr) override;
private:
    QScopedPointer<JpegDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(JpegDecoder);
};

}
}
