#include "proxy_video_decoder_impl.h"
#if defined(ENABLE_PROXY_DECODER)

#include <nx/utils/debug_utils.h>

#include "proxy_video_decoder_utils.h"
#include "aligned_mem_video_buffer.h"

namespace nx {
namespace media {

namespace {

static constexpr const char* OUTPUT_PREFIX = "ProxyVideoDecoder<yuv_planar>: ";

class Impl: public ProxyVideoDecoderImpl
{
public:
    using ProxyVideoDecoderImpl::ProxyVideoDecoderImpl;

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
        AV_PIX_FMT_YUV420P, alignedWidth, frameSize().height());
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

ProxyVideoDecoderImpl* ProxyVideoDecoderImpl::createImplYuvPlanar(const Params& params)
{
    PRINT << "Using this impl";
    return new Impl(params);
}

} // namespace media
} // namespace nx

#endif // ENABLE_PROXY_DECODER
