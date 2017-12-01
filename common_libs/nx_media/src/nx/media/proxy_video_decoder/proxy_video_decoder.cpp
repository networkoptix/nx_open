#include "proxy_video_decoder.h"
#if defined(ENABLE_PROXY_DECODER)

#include <nx/kit/debug.h>

#include "proxy_video_decoder_utils.h"
#include "proxy_video_decoder_impl.h"

namespace nx {
namespace media {

namespace {

static ProxyVideoDecoderImpl* createProxyVideoDecoderImpl(
    const ProxyVideoDecoderImpl::Params& params)
{
    int flagsCount =
        (int) ini().implStub +
        (int) ini().implDisplay +
        (int) ini().implRgb +
        (int) ini().implYuvPlanar +
        (int) ini().implYuvNative +
        (int) ini().implGl;

    if (flagsCount > 1)
    {
        NX_PRINT << "More than one impl... flag is set in .ini; using STUB w/o libproxydecoder";
        return ProxyVideoDecoderImpl::createImplStub(params);
    }

    if (flagsCount == 0)
    {
        NX_PRINT << "No impl... flag is set in .ini; using STUB w/o libproxydecoder";
        return ProxyVideoDecoderImpl::createImplStub(params);
    }

    if (ini().implStub)
        return ProxyVideoDecoderImpl::createImplStub(params);
    if (ini().implDisplay)
        return ProxyVideoDecoderImpl::createImplDisplay(params);
    if (ini().implRgb)
        return ProxyVideoDecoderImpl::createImplRgb(params);
    if (ini().implYuvPlanar)
        return ProxyVideoDecoderImpl::createImplYuvPlanar(params);
    if (ini().implYuvNative)
        return ProxyVideoDecoderImpl::createImplYuvNative(params);
    if (ini().implGl)
        return ProxyVideoDecoderImpl::createImplGl(params);

    NX_CRITICAL(false);
    return nullptr; //< Warning suppress.
}

} // namespace

ProxyVideoDecoder::ProxyVideoDecoder(
    const ResourceAllocatorPtr& allocator, const QSize& resolution)
{
    static_assert(QN_BYTE_ARRAY_PADDING >= ProxyDecoder::CompressedFrame::kPaddingSize,
        "ProxyVideoDecoder: Insufficient padding size");

    // Checked by isCompatible().
    NX_CRITICAL(resolution.width() % 2 == 0 && resolution.height() % 2 == 0);

    ini().reload();

    ProxyVideoDecoderImpl::Params params;
    params.owner = this;
    params.allocator = allocator;
    params.resolution = resolution;
    d.reset(createProxyVideoDecoderImpl(params));
}

ProxyVideoDecoder::~ProxyVideoDecoder()
{
}

bool ProxyVideoDecoder::isCompatible(
    const AVCodecID codec, const QSize& resolution, bool allowOverlay)
{
    const auto loggedCall =
        [&](bool returnValue)
        {
            return lm("isCompatible(%1, %2 x %3, allowOverlay: %4) -> %5")
                .args(avcodec_get_name(codec), resolution.width(), resolution.height(),
                    allowOverlay, returnValue);
        };

    static bool calledOnce = false;
    if (!calledOnce)
    {
        calledOnce = true;
        ini().reload();
    }

    if (ini().disable)
    {
        NX_PRINT << loggedCall(false) << ": .ini disable is set";
        return false;
    }

    if (!allowOverlay)
    {
        if (ini().alwaysAllowOverlay)
        {
            NX_PRINT << "isCompatible(): "
                << "ignoring allowOverlay being false, because .ini alwaysAllowOverlay=true";
        }
        else
        {
            NX_OUTPUT << loggedCall(false) << ": allowOverlay is false";
            return false;
        }
    }

    // Odd frame dimensions are not tested and can be unsupported due to UV having half-res.
    if (resolution.width() % 2 != 0 || resolution.height() % 2 != 0)
    {
        NX_OUTPUT << loggedCall(false) << ": only even width and height is supported";
        return false;
    }

    const QSize& maxRes = maxResolution(codec);
    if (resolution.width() > maxRes.width() || resolution.height() > maxRes.height())
    {
        NX_OUTPUT << loggedCall(false) << ": resolution is higher than "
            << maxRes.width() << " x " << maxRes.height();
        return false;
    }

    if (codec != AV_CODEC_ID_H264)
    {
        NX_OUTPUT << loggedCall(false) << ": Only codec AV_CODEC_ID_H264 is supported";
        return false;
    }

    NX_OUTPUT << loggedCall(true);
    return true;
}

QSize ProxyVideoDecoder::maxResolution(const AVCodecID /*codec*/)
{
    int w, h;
    ProxyDecoder::getMaxResolution(&w, &h);
    return QSize(w, h);
}

int ProxyVideoDecoder::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData, QVideoFramePtr* outDecodedFrame)
{
    return d->decode(compressedVideoData, outDecodedFrame);
}

virtual Capabilities ProxyVideoDecoder::capabilities() const
{
    // TODO: #mike deliver this information from proxy_decoder.h.
    return Capability::hardwareAccelerated;
}

} // namespace media
} // namespace nx

#endif // defined(ENABLE_PROXY_DECODER)
