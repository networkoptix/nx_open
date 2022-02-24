// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "proxy_video_decoder_impl.h"
#if defined(ENABLE_PROXY_DECODER)

#include <nx/kit/debug.h>

#include <nx/media/aligned_mem_video_buffer.h>

#include "proxy_video_decoder_utils.h"

extern "C" {
#include <libavutil/imgutils.h>
} // extern "C"

namespace nx {
namespace media {

namespace {

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

    const int alignedWidth = qPower2Ceil(
        (unsigned int) frameSize().width(), (unsigned int) kMediaAlignment);
    const int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, alignedWidth, frameSize().height(), /*align*/ 1);
    const int argbLineSize = alignedWidth * 4;

    auto videoBuffer = new AlignedMemVideoBuffer(numBytes, kMediaAlignment, argbLineSize);
    uchar* const argbBuffer = videoBuffer->map(QAbstractVideoBuffer::WriteOnly, nullptr, nullptr);

    auto compressedFrame = createUniqueCompressedFrame(compressedVideoData);
    int64_t ptsUs = 0;
    NX_TIME_BEGIN(DecodeToRgb);
    // Perform actual decoding to AlignedMemVideoBuffer.
    int result = proxyDecoder().decodeToRgb(compressedFrame.get(), &ptsUs, argbBuffer, argbLineSize);
    NX_TIME_END(DecodeToRgb);
    videoBuffer->unmap();

    if (result > 0)
    {
        setQVideoFrame(outDecodedFrame, videoBuffer, QVideoFrame::Format_ARGB32, ptsUs);
    }
    else
    {
        // Error or no frame (buffering).
        delete videoBuffer;
        outDecodedFrame->reset();
    }

    return result;
}

} // namespace

//-------------------------------------------------------------------------------------------------

ProxyVideoDecoderImpl* ProxyVideoDecoderImpl::createImplRgb(const Params& params)
{
    NX_PRINT << "Using this impl";
    return new Impl(params);
}

} // namespace media
} // namespace nx

#endif // defined(ENABLE_PROXY_DECODER)
