// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "jpeg_decoder.h"

#include "jpeg.h"

namespace nx {
namespace media {

//-------------------------------------------------------------------------------------------------
// JpegDecoderPrivate

class JpegDecoderPrivate: public QObject
{
public:
    JpegDecoderPrivate(): frameNumber(0) {}
    int frameNumber;
};

//-------------------------------------------------------------------------------------------------
// JpegDecoder

JpegDecoder::JpegDecoder(
    const RenderContextSynchronizerPtr& /*synchronizer*/,
    const QSize& /*resolution*/)
    :
    d_ptr(new JpegDecoderPrivate())
{
}

bool JpegDecoder::isCompatible(
    const AVCodecID codec,
     const QSize& /*resolution*/,
     bool /*allowOverlay*/,
     bool /*allowHardwareAcceleration*/)
{
    return codec == AV_CODEC_ID_MJPEG;
}

QSize JpegDecoder::maxResolution(const AVCodecID /*codec*/)
{
    return QSize();
}

int JpegDecoder::decode(const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result)
{
    Q_D(JpegDecoder);

    if (!frame)
        return 0; //< There is no internal buffer. Nothing to flush.

    QImage image = decompressJpegImage(frame->data(), frame->dataSize());
    result->reset(new QVideoFrame(image));
    (*result)->setStartTime(frame->timestamp / 1000); //< convert usec to msec
    return d->frameNumber++;
}

AbstractVideoDecoder::Capabilities JpegDecoder::capabilities() const
{
    return Capability::noCapability;
}

} // namespace media
} // namespace nx
