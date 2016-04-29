#include "jpeg_decoder.h"

#include "jpeg.h"

namespace nx {
namespace media {

//-------------------------------------------------------------------------------------------------
// JpegDecoderPrivate

class JpegDecoderPrivate : public QObject
{
public:
    JpegDecoderPrivate(): frameNumber(0) {}
    int frameNumber;
};

//-------------------------------------------------------------------------------------------------
// JpegDecoder

JpegDecoder::JpegDecoder(const ResourceAllocatorPtr& allocator, const QSize& resolution)
:
    d_ptr(new JpegDecoderPrivate())
{
    QN_UNUSED(allocator, resolution);
}

bool JpegDecoder::isCompatible(const CodecID codec, const QSize& /*resolution*/)
{
    return codec == CODEC_ID_MJPEG;
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

} // namespace media
} // namespace nx
