#include "proxy_video_decoder_private.h"
#if defined(ENABLE_PROXY_DECODER)

#include "aligned_mem_video_buffer.h"

#define OUTPUT_PREFIX "ProxyVideoDecoder<yuv_planar>: "
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

    const int alignedWidth = qPower2Ceil((unsigned) frameSize().width(), (unsigned) kMediaAlignment);
    const int numBytes = avpicture_get_size(
        PIX_FMT_YUV420P, alignedWidth, frameSize().height());
    const int lineSize = alignedWidth;

    auto videoBuffer = new AlignedMemVideoBuffer(numBytes, kMediaAlignment, lineSize);
    uchar* const buffer = videoBuffer->map(QAbstractVideoBuffer::WriteOnly, nullptr, nullptr);
    uint8_t* const yBuffer = buffer;
    uint8_t* const uBuffer = buffer + lineSize * frameSize().height();
    uint8_t* const vBuffer = uBuffer + lineSize * frameSize().height() / 4;

    auto compressedFrame = createUniqueCompressedFrame(compressedVideoData);
    int64_t ptsUs = 0;
    NX_TIME_BEGIN(decodeToYuvPlanar);
    // Perform actual decoding to AlignedMemVideoBuffer.
    int result = proxyDecoder().decodeToYuvPlanar(compressedFrame.get(), &ptsUs,
        yBuffer, lineSize, uBuffer, vBuffer, lineSize / 2);
    NX_TIME_END(decodeToYuvPlanar);
    videoBuffer->unmap();

    if (result > 0)
    {
        setQVideoFrame(outDecodedFrame, videoBuffer, QVideoFrame::Format_YUV420P, ptsUs);
    }
    else
    {
        // Error or no frame (buffering).
        outDecodedFrame->reset();
        delete videoBuffer;
    }
    return result;
}

} // namespace

//-------------------------------------------------------------------------------------------------

ProxyVideoDecoderPrivate* ProxyVideoDecoderPrivate::createImplYuvPlanar(const Params& params)
{
    PRINT << "Using this impl";
    return new Impl(params);
}

} // namespace media
} // namespace nx

#endif // ENABLE_PROXY_DECODER
