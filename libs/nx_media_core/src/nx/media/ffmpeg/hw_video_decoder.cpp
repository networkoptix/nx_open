// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hw_video_decoder.h"

#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/metric/metrics_storage.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
} // extern "C"

namespace {

static enum AVPixelFormat getHwFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    if (!ctx || !ctx->opaque)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to get HW surface format, invalid codec context");
        return AV_PIX_FMT_NONE;
    }

    auto nxDecoder = (nx::media::ffmpeg::HwVideoDecoder*)ctx->opaque;
    auto format = nxDecoder->getPixelFormat();
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++)
    {
        if (*p == format)
            return *p;
    }

    NX_DEBUG(NX_SCOPE_TAG, "Failed to get HW surface format");
    return AV_PIX_FMT_NONE;
}

}
namespace nx::media::ffmpeg {

HwVideoDecoder::HwVideoDecoder(
    AVHWDeviceType type,
    nx::metric::Storage* metrics,
    std::unique_ptr<AvOptions> options,
    InitFunc initFunc)
    :
    m_type(type),
    m_initFunc(initFunc),
    m_metrics(metrics)
{
    if (m_metrics)
        m_metrics->decoders()++;
    m_options = std::move(options);
}

HwVideoDecoder::~HwVideoDecoder()
{
    resetDecoder(nullptr);
    if (m_metrics)
        m_metrics->decoders()--;
}

bool HwVideoDecoder::isCompatible(const AVCodecID /*codec*/, const QSize& /*resolution*/, bool /*allowOverlay*/)
{
    return true;
}

bool HwVideoDecoder::isAvailable()
{
    return true;
}

QSize HwVideoDecoder::maxResolution(const AVCodecID /*codec*/)
{
    return QSize(10000, 10000);
}

bool HwVideoDecoder::initialize(const QnConstCompressedVideoDataPtr& data)
{
    if (!initializeHardware(data))
    {
        NX_DEBUG(this, "Failed to initialize hardware video decoder, try to use sofware version");
        return initializeSoftware(data);
    }
    return true;
}

bool HwVideoDecoder::initializeSoftware(const QnConstCompressedVideoDataPtr& data)
{
    auto codec = avcodec_find_decoder(data->compressionType);
    if (codec == nullptr)
    {
        NX_ERROR(this, "Failed to open ffmpeg video decoder, codec not found: %1",
            data->compressionType);
        return false;
    }
    m_decoderContext = avcodec_alloc_context3(codec);
    if (data->context)
        data->context->toAvCodecContext(m_decoderContext);

    int status = avcodec_open2(m_decoderContext, codec, NULL);
    if (status < 0)
    {
        avcodec_free_context(&m_decoderContext);
        NX_ERROR(this, "Failed to open ffmpeg video decoder, error: %1",
            nx::media::ffmpeg::avErrorToString(status));
        return false;
    }
    return true;
}

bool HwVideoDecoder::initializeHardware(const QnConstCompressedVideoDataPtr& data)
{
    if (!data || !data->context)
    {
        NX_WARNING(this, "Invalid data for HW decoder initialization");
        return false;
    }

    AVHWDeviceType typeIter = AV_HWDEVICE_TYPE_NONE;
    while((typeIter = av_hwdevice_iterate_types(typeIter)) != AV_HWDEVICE_TYPE_NONE)
        NX_DEBUG(this, "available device %1", av_hwdevice_get_type_name(typeIter));

    const AVCodec* decoder = avcodec_find_decoder(data->compressionType);
    if (decoder == 0)
    {
        NX_WARNING(this, "Could not find codec %1.", data->compressionType);
        return false;
    }

    for (int i = 0; ; i++)
    {
        const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
        if (!config)
        {
            NX_DEBUG(this, "Decoder %1 does not support device type %2.",
                    decoder->name, av_hwdevice_get_type_name(m_type));
            return false;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == m_type)
        {
            m_targetPixelFormat = config->pix_fmt;
            break;
        }
    }

    if (m_targetPixelFormat == AV_PIX_FMT_NONE)
    {
        NX_WARNING(this, "Cannot find pixel format for decoder %1 and device type %2.",
                decoder->name, av_hwdevice_get_type_name(m_type));
        return false;
    }
    else
    {
        NX_DEBUG(this, "Found pixel format %1 for decoder %2 and device type %3.",
            m_targetPixelFormat, decoder->name, av_hwdevice_get_type_name(m_type));
    }

    if (!(m_decoderContext = avcodec_alloc_context3(decoder)))
        return false;

    auto decoderCtxGuard = nx::utils::makeScopeGuard(
        [this]() { avcodec_free_context(&m_decoderContext); });

    if (avcodec_parameters_to_context(m_decoderContext, data->context->getAvCodecParameters()) < 0)
    {
        NX_WARNING(this, "Failed to convert codec parameters to codec context");
        return false;
    }

    m_decoderContext->opaque = this;
    m_decoderContext->get_format = getHwFormat;
    int status = 0;

    if (m_initFunc)
    {
        m_decoderContext->hw_device_ctx = av_hwdevice_ctx_alloc(m_type);

        auto deviceContext = (AVHWDeviceContext*) m_decoderContext->hw_device_ctx->data;

        if (!m_initFunc(deviceContext))
        {
            NX_DEBUG(this, "Failed to init HW device(custom initialization failed)");
            return false;
        }

        status = av_hwdevice_ctx_init(m_decoderContext->hw_device_ctx);
        if (status < 0)
        {
            NX_DEBUG(this, "Failed to init HW device context %1", avErrorToString(status));
            return false;
        }
    }
    else
    {
        const char* hwDevice = nullptr;
        AVDictionary* options = m_options ? m_options->get() : nullptr;
        if ((status = av_hwdevice_ctx_create(&m_hwDeviceContext, m_type, hwDevice, options, 0)) < 0)
        {
            NX_DEBUG(this, "Failed to create HW device context %1", avErrorToString(status));
            return false;
        }
        m_decoderContext->hw_device_ctx = av_buffer_ref(m_hwDeviceContext);
    }

    if ((status = avcodec_open2(m_decoderContext, decoder, NULL)) < 0)
    {
        NX_DEBUG(this, "Failed to open HW decoder %1", avErrorToString(status));
        return false;
    }

    NX_DEBUG(this, "Hardware decoder initialized: %1", decoder->name);
    decoderCtxGuard.disarm();
    return true;
}

bool HwVideoDecoder::decode(
    const QnConstCompressedVideoDataPtr& data, CLVideoDecoderOutputPtr* const outFrame)
{
    if (!m_decoderContext && !initialize(data))
    {
        NX_DEBUG(this, "Failed to initialize decoder");
        m_lastDecodeResult = -1;
        return false;
    }

    auto avPacket = av_packet_alloc();
    avPacket->data = (uint8_t*)(data->data());
    avPacket->size = data->dataSize();
    avPacket->dts = data->timestamp;
    avPacket->pts = data->timestamp;

    int status = avcodec_send_packet(m_decoderContext, avPacket);
    av_packet_free(&avPacket);

    if (status < 0)
    {
        NX_DEBUG(this, "Decoding error: %1", avErrorToString(status));
        m_lastDecodeResult = status;
        return false;
    }

    CLVideoDecoderOutput* const frame = outFrame->data();
    status = avcodec_receive_frame(m_decoderContext, frame);
    if (status == AVERROR(EAGAIN) || status == AVERROR_EOF)
    {
        m_lastDecodeResult = 0;
        return false;
    }
    else if (status < 0)
    {
        NX_DEBUG(this, "Decoding error(receive frame): %1", avErrorToString(status));
        m_lastDecodeResult = status;
        return false;
    }
    if (frame->format == m_targetPixelFormat)
        frame->setMemoryType(MemoryType::VideoMemory);

    frame->channel = data->channelNumber;
    m_lastDecodeResult = 0;
    if (m_metrics)
        m_metrics->decodedPixels() += frame->width * frame->height;
    return true;
}

bool HwVideoDecoder::resetDecoder(const QnConstCompressedVideoDataPtr& data)
{
    if (m_decoderContext)
        avcodec_free_context(&m_decoderContext);

    if (m_hwDeviceContext)
        av_buffer_unref(&m_hwDeviceContext);

    m_targetPixelFormat = AV_PIX_FMT_NONE;
    m_lastDecodeResult = 0;

    if (data)
        return initialize(data);

    return true;
}

int HwVideoDecoder::getLastDecodeResult() const
{
    return m_lastDecodeResult;
}

void HwVideoDecoder::setGreyOnlyMode(bool value)
{
    if (!m_decoderContext)
        return;

    if (value)
        m_decoderContext->flags |= AV_CODEC_FLAG_GRAY;
    else
        m_decoderContext->flags &= ~AV_CODEC_FLAG_GRAY;
}

int HwVideoDecoder::getWidth() const { return m_decoderContext ? m_decoderContext->width : 0; }
int HwVideoDecoder::getHeight() const { return m_decoderContext ? m_decoderContext->height : 0; }

} // namespace nx::media::ffmpeg
