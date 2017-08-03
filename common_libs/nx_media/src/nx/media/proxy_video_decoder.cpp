#include "proxy_video_decoder.h"
#if defined(ENABLE_PROXY_DECODER)

#define OUTPUT_PREFIX "ProxyVideoDecoder: "
#include "proxy_video_decoder_utils.h"

#include "proxy_video_decoder_private.h"

namespace nx {
namespace media {

namespace {

static ProxyVideoDecoderPrivate* createProxyVideoDecoderPrivate(
    const ProxyVideoDecoderPrivate::Params& params)
{
    int flagsCount =
        (int) conf.implDisplay +
        (int) conf.implRgb +
        (int) conf.implYuvPlanar +
        (int) conf.implYuvNative +
        (int) conf.implGl;

    if (flagsCount > 1)
    {
        PRINT << "More than one impl... configuration flag is set; using STUB w/o libproxydecoder";
        return ProxyVideoDecoderPrivate::createImplStub(params);
    }

    if (flagsCount == 0)
    {
        PRINT << "No impl... configuration flag is set; using STUB w/o libproxydecoder";
        return ProxyVideoDecoderPrivate::createImplStub(params);
    }

    if (conf.implStub)
        return ProxyVideoDecoderPrivate::createImplStub(params);
    if (conf.implDisplay)
        return ProxyVideoDecoderPrivate::createImplDisplay(params);
    if (conf.implRgb)
        return ProxyVideoDecoderPrivate::createImplRgb(params);
    if (conf.implYuvPlanar)
        return ProxyVideoDecoderPrivate::createImplYuvPlanar(params);
    if (conf.implYuvNative)
        return ProxyVideoDecoderPrivate::createImplYuvNative(params);
    if (conf.implGl)
        return ProxyVideoDecoderPrivate::createImplGl(params);

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

    conf.reload();

    ProxyVideoDecoderPrivate::Params params;
    params.owner = this;
    params.allocator = allocator;
    params.resolution = resolution;
    d.reset(createProxyVideoDecoderPrivate(params));
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
                .strs(avcodec_get_name(codec), resolution.width(), resolution.height(),
                    allowOverlay, returnValue);
        };

    static bool calledOnce = false;
    if (!calledOnce)
    {
        calledOnce = true;
        conf.reload();
        conf.skipNextReload();
    }

    if (conf.disable)
    {
        PRINT << loggedCall(false) << ": conf.disable is set";
        return false;
    }

    if (!allowOverlay)
    {
        if (!conf.largeOnly)
        {
            PRINT << "isCompatible() ignores allowOverlay because conf.largeOnly is set";
        }
        else
        {
            OUTPUT << loggedCall(false) << ": allowOverlay is false";
            return false;
        }
    }

    // Odd frame dimensions are not tested and can be unsupported due to UV having half-res.
    if (resolution.width() % 2 != 0 || resolution.height() % 2 != 0)
    {
        OUTPUT << loggedCall(false) << ": only even width and height is supported";
        return false;
    }

    const QSize& maxRes = maxResolution(codec);
    if (resolution.width() > maxRes.width() || resolution.height() > maxRes.height())
    {
        OUTPUT << loggedCall(false) << ": resolution is higher than "
            << maxRes.width() << " x " << maxRes.height();
        return false;
    }

    if (codec != AV_CODEC_ID_H264)
    {
        OUTPUT << loggedCall(false) << ": Only codec AV_CODEC_ID_H264 is supported";
        return false;
    }

    OUTPUT << loggedCall(true);
    return true;
}

QSize ProxyVideoDecoder::maxResolution(const AVCodecID codec)
{
    QN_UNUSED(codec);

    int w, h;
    ProxyDecoder::getMaxResolution(&w, &h);
    return QSize(w, h);
}

int ProxyVideoDecoder::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData, QVideoFramePtr* outDecodedFrame)
{
    return d->decode(compressedVideoData, outDecodedFrame);
}

} // namespace media
} // namespace nx

#endif // defined(ENABLE_PROXY_DECODER)
