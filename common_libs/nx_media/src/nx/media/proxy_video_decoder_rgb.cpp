#include "proxy_video_decoder_private.h"
#if defined(ENABLE_PROXY_DECODER)

#include "aligned_mem_video_buffer.h"
#include "proxy_video_decoder_utils.h"

namespace nx {
namespace media {

namespace {

class Impl
:
    public ProxyVideoDecoderPrivate
{
public:
    using ProxyVideoDecoderPrivate::ProxyVideoDecoderPrivate;

    virtual int decode(
        const QnConstCompressedVideoDataPtr& compressedVideoData,
        QVideoFramePtr* outDecodedFrame) override;
};

int Impl::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData,
    QVideoFramePtr* outDecodedFrame)
{
    NX_CRITICAL(outDecodedFrame);

    const int alignedWidth = qPower2Ceil(
        (unsigned int) frameSize().width(), (unsigned int) kMediaAlignment);
    const int numBytes = avpicture_get_size(PIX_FMT_BGRA, alignedWidth, frameSize().height());
    const int argbLineSize = alignedWidth * 4;

    auto alignedBuffer = new AlignedMemVideoBuffer(numBytes, kMediaAlignment, argbLineSize);

    outDecodedFrame->reset(new QVideoFrame(alignedBuffer, frameSize(), QVideoFrame::Format_RGB32));
    QVideoFrame* const decodedFrame = outDecodedFrame->get();

    decodedFrame->map(QAbstractVideoBuffer::WriteOnly);
    uchar* argbBuffer = decodedFrame->bits();

    auto compressedFrame = createUniqueCompressedFrame(compressedVideoData);
    int64_t outPts = 0;
    int result;
    TIME_BEGIN("decodeToRgb")
    // Perform actual decoding from QnCompressedVideoData to QVideoFrame.
    result = proxyDecoder().decodeToRgb(compressedFrame.get(), &outPts, argbBuffer, argbLineSize);
    TIME_END

    decodedFrame->unmap();

    if (result <= 0) //< Error or no frame (buffering).
        outDecodedFrame->reset();
    else
        decodedFrame->setStartTime(outPts);

    return result;
}

} // namespace

//-------------------------------------------------------------------------------------------------

ProxyVideoDecoderPrivate* ProxyVideoDecoderPrivate::createImplRgb(
    ProxyVideoDecoder* owner, const ResourceAllocatorPtr& allocator, const QSize& resolution)
{
    qDebug() << "ProxyVideoDecoder: Using 'rgb' impl";
    return new Impl(owner, allocator, resolution);
}

} // namespace media
} // namespace nx

#endif // ENABLE_PROXY_DECODER
