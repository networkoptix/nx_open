// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hw_video_decoder.h"
#include "shared_memory_frame_allocator.h"

#include <utility>

#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/metric/metrics_storage.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/cpu.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
} // extern "C"

namespace nx::media::ffmpeg {

static bool isHwPixelFormat(AVPixelFormat format)
{
    // Use FFmpeg descriptor flags instead of a hardcoded list so all HW formats are covered
    // (for example DXVA2/QSV/CUDA and any future additions).
    const AVPixFmtDescriptor* const descriptor = av_pix_fmt_desc_get(format);
    return descriptor && (descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL);
}

static enum AVPixelFormat getHwFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    if (!ctx || !ctx->opaque)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to get HW surface format, invalid codec context");
        return AV_PIX_FMT_NONE;
    }

    const auto* const bufferContext =
        static_cast<const nx::media::ffmpeg::FfmpegSharedMemoryBufferContext*>(ctx->opaque);
    if (bufferContext->magic != nx::media::ffmpeg::FfmpegSharedMemoryBufferContext::kMagic
        || !bufferContext->owner)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to get HW surface format, invalid buffer context");
        return AV_PIX_FMT_NONE;
    }

    auto nxDecoder = static_cast<nx::media::ffmpeg::HwVideoDecoder*>(bufferContext->owner);
    auto format = nxDecoder->getPixelFormat();
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++)
    {
        if (*p == format)
            return *p;
    }

    if (pix_fmts)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to get HW surface format, use software one: %1", *pix_fmts);
        return *pix_fmts;
    }

    NX_DEBUG(NX_SCOPE_TAG, "Failed to get HW surface format");
    return AV_PIX_FMT_NONE;
}

static AVPixelFormat getHwPixelFormatForCodec(const AVCodec* codec, AVHWDeviceType deviceType)
{
    for (int i = 0; ; i++)
    {
        const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
        if (!config)
            return AV_PIX_FMT_NONE;

        if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
            && config->device_type == deviceType)
        {
            return config->pix_fmt;
        }
    }
}

std::tuple<const AVCodec*, AVPixelFormat> findHwDecoderForCodec(
    AVCodecID codecId,
    AVHWDeviceType deviceType)
{
    void* opaque = nullptr;
    const AVCodec* codec = nullptr;

    while ((codec = av_codec_iterate(&opaque)))
    {
        if (!av_codec_is_decoder(codec))
            continue;
        if (codec->id != codecId)
            continue;

        auto pixelFormat = getHwPixelFormatForCodec(codec, deviceType);
        if (pixelFormat != AV_PIX_FMT_NONE)
            return {codec, pixelFormat};

        NX_DEBUG(NX_SCOPE_TAG, "Codec %1 can decode %2 only in SW",
            codec->name, avcodec_get_name(codecId));
    }

    return {nullptr, AV_PIX_FMT_NONE};
}

HwVideoDecoder::HwVideoDecoder(
    AVHWDeviceType type,
    nx::metric::Storage* metrics,
    const std::string& device,
    std::unique_ptr<AvOptions> options,
    InitFunc initFunc,
    FfmpegSharedMemoryAllocatorPtr sharedMemoryAllocator,
    bool useSharedMemoryForSoftwareFallback)
    :
    m_type(type),
    m_initFunc(initFunc),
    m_metrics(metrics),
    m_sharedMemoryAllocator(std::move(sharedMemoryAllocator)),
    m_useSharedMemoryForSoftwareFallback(useSharedMemoryForSoftwareFallback)
{
    if (m_metrics)
        m_metrics->decoders()++;
    m_device = device;
    m_options = std::move(options);
}

HwVideoDecoder::~HwVideoDecoder()
{
    resetDecoder(nullptr);
    if (m_metrics)
        m_metrics->decoders()--;
}

bool HwVideoDecoder::isCompatible(
    const AVCodecID /*codec*/,
    const QSize& /*resolution*/,
    bool /*allowHardwareAcceleration*/)
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
    if (!data)
    {
        NX_DEBUG(this, "Cannot initialize video decoder without a data packet");
        return false;
    }

    if (!initializeHardware(data))
    {
        // If HW init fails we fallback to software decode in the same decoder instance.
        // Isolated-mode SHM behavior is handled inside initializeSoftware().
        NX_DEBUG(this, "Failed to initialize hardware video decoder, try to use software version");
        return initializeSoftware(data);
    }
    return true;
}

bool HwVideoDecoder::initializeSoftware(const QnConstCompressedVideoDataPtr& data)
{
    m_bufferContext.reset();

    auto codec = avcodec_find_decoder(data->compressionType);
    if (codec == nullptr)
    {
        NX_ERROR(this, "Failed to open ffmpeg video decoder, codec not found: %1",
            data->compressionType);
        return false;
    }
    m_decoderContext = avcodec_alloc_context3(codec);
    if (!m_decoderContext)
    {
        NX_ERROR(this, "Failed to allocate ffmpeg software video decoder context");
        return false;
    }
    if (data->context)
        data->context->toAvCodecContext(m_decoderContext);

    if (m_useSharedMemoryForSoftwareFallback)
    {
        // Important for isolated plugins: when HW decoder falls back to SW decode, FFmpeg should
        // allocate output frame planes in FfmpegSharedMemory from the beginning.
        m_decoderContext->get_buffer2 = getFfmpegSharedMemoryBuffer;
        m_bufferContext = std::make_unique<FfmpegSharedMemoryBufferContext>();
        m_bufferContext->owner = this;
        m_bufferContext->allocator = m_sharedMemoryAllocator;
        m_decoderContext->opaque = m_bufferContext.get();
    }

    if (m_mtDecodingPolicy != MultiThreadDecodePolicy::disabled)
    {
        m_decoderContext->thread_count = std::min(kMaxDecodeThread, av_cpu_count());
        NX_DEBUG(this, "Hardware decoder thread_count: %1(used in case of software fallback)",
            m_decoderContext->thread_count);
    }

    int status = avcodec_open2(m_decoderContext, codec, NULL);
    if (status < 0)
    {
        avcodec_free_context(&m_decoderContext);
        NX_ERROR(this, "Failed to open ffmpeg video decoder, error: %1",
            nx::media::ffmpeg::avErrorToString(status));
        return false;
    }

    m_hardwareMode = false;

    return true;
}

bool HwVideoDecoder::initializeHardware(const QnConstCompressedVideoDataPtr& data)
{
    m_bufferContext.reset();

    if (!data || !data->context)
    {
        NX_WARNING(this, "Invalid data for HW decoder initialization");
        return false;
    }

    AVHWDeviceType typeIter = AV_HWDEVICE_TYPE_NONE;
    while((typeIter = av_hwdevice_iterate_types(typeIter)) != AV_HWDEVICE_TYPE_NONE)
        NX_DEBUG(this, "available device %1", av_hwdevice_get_type_name(typeIter));

    NX_DEBUG(this, "Trying to initialize hardware decoder, type: %1, codec: %2",
        av_hwdevice_get_type_name(m_type), data->compressionType);

    auto [decoder, pixelFormat] = findHwDecoderForCodec(data->compressionType, m_type);

    if (pixelFormat == AV_PIX_FMT_NONE)
    {
        NX_WARNING(this, "Cannot find HW decoder for codec %1 (%2) and device type %3.",
            data->compressionType,
            avcodec_get_name(data->compressionType),
            av_hwdevice_get_type_name(m_type));
        // Codec has no HW path on this platform - not a resource failure, no back-off needed.
        return false;
    }
    // From here a HW decoder exists for this codec. Any allocation or device failure is
    // resource-related and must arm the process-wide back-off via m_hardwareResourceFailed.
    // The only exception is avcodec_parameters_to_context: that is a per-stream config error,
    // not resource exhaustion, so its failure path disarms the guard explicitly.
    auto resourceFailedGuard = nx::utils::makeScopeGuard(
        [this]() { m_hardwareResourceFailed = true; });

    m_targetPixelFormat = pixelFormat;

    NX_DEBUG(this, "Found pixel format %1 for decoder %2 and device type %3.",
        m_targetPixelFormat, decoder->name, av_hwdevice_get_type_name(m_type));

    if (!(m_decoderContext = avcodec_alloc_context3(decoder)))
        return false;

    auto decoderCtxGuard = nx::utils::makeScopeGuard(
        [this]() { avcodec_free_context(&m_decoderContext); });

    if (avcodec_parameters_to_context(m_decoderContext, data->context->getAvCodecParameters()) < 0)
    {
        NX_WARNING(this, "Failed to convert codec parameters to codec context");
        resourceFailedGuard.disarm();
        return false;
    }

    m_bufferContext = std::make_unique<FfmpegSharedMemoryBufferContext>();
    m_bufferContext->owner = this;
    m_bufferContext->allocator = m_sharedMemoryAllocator;

    m_decoderContext->opaque = m_bufferContext.get();
    m_decoderContext->get_format = getHwFormat;
    int status = 0;

    if (m_initFunc)
    {
        m_decoderContext->hw_device_ctx = av_hwdevice_ctx_alloc(m_type);
        if (!m_decoderContext->hw_device_ctx)
        {
            NX_ERROR(this, "Failed to allocate HW device context");
            return false;
        }

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
        const char* hwDevice = m_device.empty() ? nullptr : m_device.c_str();
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

    m_hardwareMode = true;
    NX_DEBUG(this, "Hardware decoder initialized: %1", decoder->name);
    decoderCtxGuard.disarm();
    resourceFailedGuard.disarm();
    return true;
}

bool HwVideoDecoder::sendPacket(const QnConstCompressedVideoDataPtr& data)
{
    if (m_decoderContext && !avcodec_is_open(m_decoderContext))
    {
        // Android ResourceManager can reclaim HW codec resources under pressure, which
        // invalidates avctx->internal without freeing the AVCodecContext itself. Detect
        // this via avcodec_is_open() and reinitialize so we don't crash inside FFmpeg.
        NX_WARNING(this, "Decoder context became invalid (resource reclaim?), reinitializing");
        if (!resetDecoder(data))
        {
            m_lastDecodeResult = -1;
            return false;
        }
    }

    if (!m_decoderContext && !initialize(data))
    {
        NX_DEBUG(this, "Failed to initialize decoder");
        m_lastDecodeResult = -1;
        return false;
    }

    AVPacket* avPacket = nullptr;
    if (data)
    {
        avPacket = av_packet_alloc();
        avPacket->data = (uint8_t*)(data->data());
        avPacket->size = data->dataSize();
        avPacket->dts = data->timestamp;
        avPacket->pts = data->timestamp;

        m_channel = data->channelNumber;
        m_flags = data->flags;
    }

    int status = avcodec_send_packet(m_decoderContext, avPacket);
    av_packet_free(&avPacket);

    // We use a null packet to flush the decoder. Subsequent calls to avcodec_receive_frame() with
    // the null packet will return AVERROR_EOF, but we still need to receive the remaining frames.
    const bool allowFailure = !data && status == AVERROR_EOF;

    if (status < 0 && !allowFailure)
    {
        NX_DEBUG(this, "Decoding error: %1", avErrorToString(status));
        m_lastDecodeResult = status;
        return false;
    }
    return true;
}

bool HwVideoDecoder::receiveFrame(CLVideoDecoderOutputPtr* const outFrame)
{
    if (!m_decoderContext || !avcodec_is_open(m_decoderContext))
    {
        NX_WARNING(this, "Decoder context is not open, cannot receive frame");
        m_lastDecodeResult = AVERROR(EINVAL);
        return false;
    }

    CLVideoDecoderOutput* const frame = outFrame->data();
    int status = avcodec_receive_frame(m_decoderContext, frame);
    if (status == AVERROR(EAGAIN) || status == AVERROR_EOF)
    {
        m_lastDecodeResult = 0;
        return true;
    }
    else if (status < 0)
    {
        NX_DEBUG(this, "Decoding error(receive frame): %1", avErrorToString(status));
        m_lastDecodeResult = status;
        return false;
    }
    // This is the canonical place where decoded frame memory type is decided.
    // Downstream motion/analytics code relies on MemoryType::VideoMemory and does not perform
    // extra hw_frames_ctx-based fallback checks.
    // Do not rely only on `format == m_targetPixelFormat` here: in fallback/hybrid paths FFmpeg
    // may still return hardware-backed frames where `hw_frames_ctx` is present or the format is a
    // known HW pixel format.
    const bool isHardwareSurface =
        frame->format == m_targetPixelFormat
        || (bool) frame->hw_frames_ctx
        || isHwPixelFormat((AVPixelFormat) frame->format);

    if (isHardwareSurface)
        frame->setMemoryType(MemoryType::VideoMemory);

    frame->channel = m_channel;
    frame->flags = m_flags;
    m_lastDecodeResult = 0;
    if (m_metrics)
        m_metrics->decodedPixels() += frame->width * frame->height;

    m_hardwareMode = isHardwareSurface;
    return true;
}

bool HwVideoDecoder::decode(
    const QnConstCompressedVideoDataPtr& data, CLVideoDecoderOutputPtr* const outFrame)
{
    if (!sendPacket(data))
        return false;

    if (!receiveFrame(outFrame) || (outFrame && (*outFrame)->format == -1))
        return false;

    return true;
}

int HwVideoDecoder::currentFrameNumber() const
{
    if (!m_decoderContext)
        return 0;

    return std::max<int64_t>(0, m_decoderContext->frame_num - 1);
}

bool HwVideoDecoder::resetDecoder(const QnConstCompressedVideoDataPtr& data)
{
    if (m_decoderContext)
        avcodec_free_context(&m_decoderContext);

    if (m_hwDeviceContext)
        av_buffer_unref(&m_hwDeviceContext);

    m_bufferContext.reset();
    m_targetPixelFormat = AV_PIX_FMT_NONE;
    m_lastDecodeResult = 0;
    m_hardwareResourceFailed = false;

    if (data)
        return initialize(data);

    return true;
}

bool HwVideoDecoder::consumeHardwareResourceFailed()
{
    const bool result = m_hardwareResourceFailed;
    m_hardwareResourceFailed = false;
    return result;
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

void HwVideoDecoder::setMultiThreadDecodePolicy(MultiThreadDecodePolicy policy)
{
    m_mtDecodingPolicy = policy;
}

int HwVideoDecoder::getWidth() const { return m_decoderContext ? m_decoderContext->width : 0; }
int HwVideoDecoder::getHeight() const { return m_decoderContext ? m_decoderContext->height : 0; }
AVCodecID HwVideoDecoder::codec() const
{
    return m_decoderContext ? m_decoderContext->codec_id : AV_CODEC_ID_NONE;
};

} // namespace nx::media::ffmpeg
