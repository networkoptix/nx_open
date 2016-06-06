#include "proxy_video_decoder_private.h"
#if defined(ENABLE_PROXY_DECODER)

#include "aligned_mem_video_buffer.h"

#define OUTPUT_PREFIX "ProxyVideoDecoder<STUB>: "
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
    Q_UNUSED(compressedVideoData);

    static int frameNumber = 0;

    NX_CRITICAL(outDecodedFrame);

    const int alignedWidth = qPower2Ceil(
        (unsigned int) frameSize().width(), (unsigned int) kMediaAlignment);
    const int numBytes = avpicture_get_size(PIX_FMT_BGRA, alignedWidth, frameSize().height());
    const int argbLineSize = alignedWidth * 4;

    if (compressedVideoData)
    {
        auto videoBuffer = new AlignedMemVideoBuffer(numBytes, kMediaAlignment, argbLineSize);

        uchar* const argbBuffer =
            videoBuffer->map(QAbstractVideoBuffer::WriteOnly, nullptr, nullptr);

        debugDrawCheckerboardArgb(
            argbBuffer, argbLineSize, frameSize().width(), frameSize().height());

        videoBuffer->unmap();

        setQVideoFrame(outDecodedFrame, videoBuffer, QVideoFrame::Format_RGB32,
            compressedVideoData->timestamp);

        return ++frameNumber;
    }
    else
    {
        return 0;
    }
}

} // namespace

//-------------------------------------------------------------------------------------------------

ProxyVideoDecoderPrivate* ProxyVideoDecoderPrivate::createImplStub(const Params& params)
{
    PRINT << "Using this impl";
    return new Impl(params);
}

} // namespace media
} // namespace nx

#endif // ENABLE_PROXY_DECODER
