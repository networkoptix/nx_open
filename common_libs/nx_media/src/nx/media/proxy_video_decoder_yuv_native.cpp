#include "proxy_video_decoder_private.h"
#if defined(ENABLE_PROXY_DECODER)

#include "aligned_mem_video_buffer.h"

#define OUTPUT_PREFIX "ProxyVideoDecoder<yuv_native>: "
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

    uint8_t* nativeBuffer = nullptr;
    int nativeBufferSize = 0;

    auto compressedFrame = createUniqueCompressedFrame(compressedVideoData);
    int64_t outPts = 0;
    int result = -1;
    TIME_BEGIN(decodeToYuvNative);
    // Perform actual decoding from QnCompressedVideoData to memory.
    result = proxyDecoder().decodeToYuvNative(
        compressedFrame.get(), &outPts, &nativeBuffer, &nativeBufferSize);
    TIME_END(decodeToYuvNative);

    if (result > 0)
    {
        NX_CRITICAL(nativeBuffer);
        NX_CRITICAL(nativeBufferSize > 0);

        const QVideoFrame::PixelFormat Format_Tiled32x32NV12 =
            QVideoFrame::PixelFormat(QVideoFrame::Format_User + 17);

        quint8* srcData[4];
        srcData[0] = (quint8*) nativeBuffer;
        srcData[1] = srcData[0] + qPower2Ceil(
            (unsigned int) frameSize().width(), 32)
            * qPower2Ceil((unsigned int) frameSize().height(), 32);

        int srcLineSize[4];
        srcLineSize[0] = srcLineSize[1] = qPower2Ceil((unsigned int) frameSize().width(), 32);

        auto buffer = new AlignedMemVideoBuffer(srcData, srcLineSize, /*planeCount */ 2);
        outDecodedFrame->reset(new QVideoFrame(buffer, frameSize(), Format_Tiled32x32NV12));
    }

    if (result <= 0) //< Error or no frame (buffering).
        outDecodedFrame->reset();
    else
        (*outDecodedFrame)->setStartTime(outPts);

    return result;
}

} // namespace

//-------------------------------------------------------------------------------------------------

ProxyVideoDecoderPrivate* ProxyVideoDecoderPrivate::createImplYuvNative(const Params& params)
{
    PRINT << "Using this impl";
    return new Impl(params);
}

} // namespace media
} // namespace nx

#endif // ENABLE_PROXY_DECODER
