// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "jpeg_decoder.h"

#include <nx/media/video_frame.h>

#include "jpeg.h"

namespace nx::media {

//-------------------------------------------------------------------------------------------------
// JpegDecoderPrivate

class JpegDecoderPrivate: public QObject
{
public:
    JpegDecoderPrivate(): frameNumber(0) {}
    int frameNumber;
    VideoFramePtr result;
};

//-------------------------------------------------------------------------------------------------
// JpegDecoder

JpegDecoder::JpegDecoder(
    const QSize& /*resolution*/,
    QRhi* /*rhi*/)
    :
    d_ptr(new JpegDecoderPrivate())
{
}

JpegDecoder::~JpegDecoder()
{
}

bool JpegDecoder::isCompatible(
    const AVCodecID codec,
     const QSize& /*resolution*/,
     bool /*allowHardwareAcceleration*/)
{
    return codec == AV_CODEC_ID_MJPEG;
}

QSize JpegDecoder::maxResolution(const AVCodecID /*codec*/)
{
    return QSize();
}

bool JpegDecoder::sendPacket(const QnConstCompressedVideoDataPtr& packet)
{
    Q_D(JpegDecoder);

    if (!packet)
        return 0; //< There is no internal buffer. Nothing to flush.

    const QImage image = decompressJpegImage(packet->data(), packet->dataSize());

    d->result = std::make_shared<VideoFrame>(image);

    if (!d->result)
        return 0;

    d->result->setStartTime(packet->timestamp / 1000); //< convert usec to msec
    return d->frameNumber++;
}

bool JpegDecoder::receiveFrame(VideoFramePtr* decodedFrame)
{
    Q_D(JpegDecoder);

    if (d->result)
        *decodedFrame = d->result;
    d->result.reset();
    return true;
}

int JpegDecoder::currentFrameNumber() const
{
    Q_D(const JpegDecoder);

    return d->frameNumber;
}

AbstractVideoDecoder::Capabilities JpegDecoder::capabilities() const
{
    return Capability::noCapability;
}

} // namespace nx::media
