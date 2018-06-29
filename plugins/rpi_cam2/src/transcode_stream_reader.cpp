#include "transcode_stream_reader.h"

#ifdef __linux__
#include <errno.h>
#endif

extern "C" {
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
} // extern "C"

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "utils/utils.h"
#include "ffmpeg/stream_reader.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"
#include "ffmpeg/error.h"

namespace nx {
namespace rpi_cam2 {

namespace {

int decode(const std::unique_ptr<nx::ffmpeg::Codec>& codec, AVFrame * outFrame, const nx::ffmpeg::Packet* packet)
{
    int decodeCode = 0;
    int gotFrame = 0;
    while(!gotFrame && decodeCode >= 0)
        decodeCode = codec->decode(outFrame, &gotFrame, packet->packet());
    return decodeCode;
}

}

TranscodeStreamReader::TranscodeStreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    nxpl::TimeProvider *const timeProvider,
    const nxcip::CameraInfo& cameraInfo,
    const CodecContext& codecContext,
    const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader)
:
    StreamReader(
        parentRefManager,
        timeProvider,
        cameraInfo,
        codecContext,
        ffmpegStreamReader)
{
    debug("%s\n", __FUNCTION__);
}

TranscodeStreamReader::~TranscodeStreamReader()
{
    uninitialize();
}

int TranscodeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    if(!ensureInitialized())
        return nxcip::NX_OTHER_ERROR;

    ffmpeg::Packet packet;
    if(m_ffmpegStreamReader->loadNextData(&packet) < 0)
        return nxcip::NX_IO_ERROR;

    m_decodedFrame->unreference();
    int decodeCode = decode(m_ffmpegStreamReader->decoder(), m_decodedFrame->frame(), &packet);
    if(decodeCode < 0)
        return nxcip::NX_NO_DATA;

    int scaleCode = scale(m_decodedFrame->frame(), m_scaledFrame->frame());
    if(scaleCode < 0)
        return nxcip::NX_OTHER_ERROR;

    nx::ffmpeg::Packet encodePacket;
    int encodeCode = encode(m_scaledFrame->frame(), encodePacket.packet());
    if(encodeCode < 0)
        return nxcip::NX_OTHER_ERROR;

    *lpPacket = toNxPacket(encodePacket.packet(), m_videoEncoder->codecID()).release();

    return nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::setFps( int fps )
{
    if (m_codecContext.fps() != fps)
    {
        debug("TranscodeStreamReader::setFps: %d\n", fps);
        m_codecContext.setFps(fps);
        m_modified = true;
    }
}

void TranscodeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    auto currentRes = m_codecContext.resolution();
    if(currentRes.width != resolution.width
        || currentRes.height != resolution.height)
    {
        debug("TranscodeStreamReader::setResolution: %d, %d\n", resolution.width, resolution.height);
        m_codecContext.setResolution(resolution);
        m_modified = true;
    }
}

void TranscodeStreamReader::setBitrate(int bitrate)
{
    if(m_codecContext.bitrate() != bitrate)
    {
        debug("TranscodeStreamReader::setBitrate: %d\n", bitrate);
        m_codecContext.setBitrate(bitrate);
        m_modified = true;
    }
}

/*!
 * Scale @param frame, modifying the preallocated @outFrame whose size and format are expected
 * to have already been set.
 * */
int TranscodeStreamReader::scale(AVFrame * frame, AVFrame* outFrame) const
{
    const AVPixelFormat* supportedFormats = m_videoEncoder->codec()->pix_fmts;
    
    AVPixelFormat encoderFormat = supportedFormats
        ? nx::ffmpeg::utils::unDeprecatePixelFormat(supportedFormats[0])
        : nx::ffmpeg::utils::suggestPixelFormat(m_videoEncoder->codecID());

    auto targetRes = m_codecContext.resolution();

    struct SwsContext * imageConvertContext = sws_getCachedContext(
        nullptr,
        frame->width,
        frame->height,
        nx::ffmpeg::utils::unDeprecatePixelFormat(m_ffmpegStreamReader->decoder()->pixelFormat()),
        targetRes.width,
        targetRes.height,
        encoderFormat,
        SWS_BICUBIC,
        nullptr,
        nullptr,
        nullptr);
    if(*imageConvertContext)
    {
        ffmpeg::error::setLastError(AVERROR(ENOMEM));
        return AVERROR(ENOMEM);
    }
        
    int scaleCode = sws_scale(
        imageConvertContext,
        frame->data,
        frame->linesize,
        0,
        frame->height,
        outFrame->data,
        outFrame->linesize);
    if(nx::ffmpeg::error::updateIfError(scaleCode))
        return scaleCode;

    sws_freeContext(imageConvertContext);

    return 0;
}

int TranscodeStreamReader::encode(const AVFrame * frame, AVPacket * outPacket) const
{
    int encodeCode = 0;
    int gotPacket = 0;
    while(!gotPacket && encodeCode >= 0)
        encodeCode = m_videoEncoder->encode(outPacket, frame, &gotPacket);
    return encodeCode;
}

int TranscodeStreamReader::initialize()
{
    int openCode = openVideoEncoder();
    if (openCode < 0)
        return openCode;

    int initCode = initializeScaledFrame(m_videoEncoder);
    if(initCode < 0)
        return initCode;

    auto decodedFrame = std::make_unique<ffmpeg::Frame>();
    if(!decodedFrame || !decodedFrame->frame())
        return AVERROR(ENOMEM);
    m_decodedFrame = std::move(decodedFrame);

    m_initialized = true;
    return 0;
}

int TranscodeStreamReader::openVideoEncoder()
{
    NX_DEBUG(this) << "openVideoEncoder()";

    auto encoder = std::make_unique<ffmpeg::Codec>();
   
    //todo initialize the right decoder for windows
    int initCode = encoder->initializeEncoder("h264_omx");
    if (initCode < 0)
        return initCode;

    setEncoderOptions(encoder);

    int openCode = encoder->open();
    if (openCode < 0)
        return openCode;

    m_videoEncoder = std::move(encoder);
    return 0;
}

int TranscodeStreamReader::initializeScaledFrame(const std::unique_ptr<ffmpeg::Codec>& encoder)
{
    NX_ASSERT(encoder);

    const AVPixelFormat* supportedFormats = encoder->codec()->pix_fmts;
    AVPixelFormat encoderFormat = supportedFormats
        ? nx::ffmpeg::utils::unDeprecatePixelFormat(supportedFormats[0])
        : nx::ffmpeg::utils::suggestPixelFormat(encoder->codecID());

    auto scaledFrame = std::make_unique<ffmpeg::Frame>();
    if (!scaledFrame || !scaledFrame->frame())
        return AVERROR(ENOMEM);

    auto resolution = m_codecContext.resolution();
    int allocCode = scaledFrame->imageAlloc(resolution.width, resolution.height, encoderFormat, 32);
    if (allocCode < 0)
        return allocCode;

    m_scaledFrame = std::move(scaledFrame);
    return 0;
}

void TranscodeStreamReader::setEncoderOptions(const std::unique_ptr<nx::ffmpeg::Codec>& encoder)
{
    AVCodecContext* encoderContext = encoder->codecContext();
    encoderContext->width = m_codecContext.resolution().width;
    encoderContext->height = m_codecContext.resolution().height;
    encoderContext->time_base = { 1, m_codecContext.fps() };
    encoderContext->framerate = { m_codecContext.fps(), 1 };
    encoderContext->bit_rate = m_codecContext.bitrate();
    encoderContext->pix_fmt = nx::ffmpeg::utils::suggestPixelFormat(encoderContext->codec_id);

    /* don't use global header. the rpi cam doesn't stream properly with global. */
    encoderContext->flags = AV_CODEC_FLAG2_LOCAL_HEADER;
    encoderContext->global_quality = encoderContext->qmin * FF_QP2LAMBDA;
}

void TranscodeStreamReader::uninitialize()
{
    m_decodedFrame.reset(nullptr);
    m_scaledFrame.reset(nullptr);
    m_videoEncoder.reset(nullptr);
    m_initialized = false;
}

bool TranscodeStreamReader::ensureInitialized()
{
    auto printError =
        [this]()
        {
            if(!nx::ffmpeg::error::hasError())
                return;

#ifdef __linux__
            int err = errno;
            NX_DEBUG(this) << "ensureInitialized()::errno: " << err;
#endif
            NX_DEBUG(this) << "ensureInitialized()::m_lastAvError" << nx::ffmpeg::error::lastError();
        };

    if(!m_initialized)
    {
        initialize();
        NX_DEBUG(this) << "ensureInitialized(): first initialization";
        printError();
    }
    else if(m_modified)
    {
        NX_DEBUG(this) << "ensureInitialized(): codec parameters modified, reinitializing";
        uninitialize();
        initialize();
        printError();
        m_modified = false;
    }

    return m_initialized;
}

} // namespace rpi_cam2
} // namespace nx
