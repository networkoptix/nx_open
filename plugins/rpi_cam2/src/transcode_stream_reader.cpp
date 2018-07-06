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

#include "utils/time_profiler.h"

namespace nx {
namespace rpi_cam2 {

namespace {

AVPixelFormat suggestPixelFormat(const std::unique_ptr<ffmpeg::Codec>& encoder)
{
    const AVPixelFormat* supportedFormats = encoder->codec()->pix_fmts;
    return supportedFormats
        ? ffmpeg::utils::unDeprecatePixelFormat(supportedFormats[0])
        : ffmpeg::utils::suggestPixelFormat(encoder->codecID());
}

bool scaleContextInit = false;

}

TranscodeStreamReader::TranscodeStreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    nxpl::TimeProvider *const timeProvider,
    const nxcip::CameraInfo& cameraInfo,
    const CodecContext& codecContext,
    const std::shared_ptr<ffmpeg::StreamReader>& ffmpegStreamReader)
:
    StreamReader(
        parentRefManager,
        timeProvider,
        cameraInfo,
        codecContext,
        ffmpegStreamReader)
{
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

    std::shared_ptr<ffmpeg::Packet> packet = nullptr;
    while(!packet)
        packet = m_consumer->popNextPacket();

    m_decodedFrame->unreference();
    int decodeCode = decode(m_decodedFrame->frame(), packet->packet());
    if(decodeCode < 0)
        return nxcip::NX_NO_DATA;

    int scaleCode = scale(m_decodedFrame->frame(), m_scaledFrame->frame());
    if(scaleCode < 0)
        return nxcip::NX_OTHER_ERROR;

    ffmpeg::Packet encodePacket;
    int encodeCode = encode(m_scaledFrame->frame(), encodePacket.packet());
    if(encodeCode < 0)
        return nxcip::NX_OTHER_ERROR;

    *lpPacket = toNxPacket(encodePacket.packet(), m_videoEncoder->codecID()).release();

    return nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::setFps(int fps)
{
    if (m_codecContext.fps() != fps)
    {
        StreamReader::setFps(fps);
        m_state = kModified;
    }
}

void TranscodeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    auto currentRes = m_codecContext.resolution();
    if (currentRes.width != resolution.width
        || currentRes.height != resolution.height)
    {
        StreamReader::setResolution(resolution);
        m_state = kModified;
    }
}

void TranscodeStreamReader::setBitrate(int bitrate)
{
    if (m_codecContext.bitrate() != bitrate)
    {
        StreamReader::setBitrate(bitrate);
        m_state = kModified;
    }
}

/*!
 * Scale @param frame, modifying the preallocated @param outFrame whose size and format are expected
 * to have already been set.
 * */
int TranscodeStreamReader::scale(AVFrame * frame, AVFrame* outFrame)
{
    nxcip::Resolution targetRes = m_codecContext.resolution();

    nx::rpi_cam2::utils::TimeProfiler t;
    t.start();

    if (!scaleContextInit)
    {
        scaleContextInit = true;
        m_scaleContext = sws_getCachedContext(
            nullptr,
            frame->width,
            frame->height,
            ffmpeg::utils::unDeprecatePixelFormat(m_decoder->pixelFormat()),
            targetRes.width,
            targetRes.height,
            suggestPixelFormat(m_videoEncoder),
            SWS_BICUBIC,
            nullptr,
            nullptr,
            nullptr);
        if (!m_scaleContext)
        {
            ffmpeg::error::setLastError(AVERROR(ENOMEM));
            return AVERROR(ENOMEM);
        }
    }        

    if (!m_scaleContext)
        return AVERROR(ENOMEM);

    int scaleCode = sws_scale(
        m_scaleContext,
        frame->data,
        frame->linesize,
        0,
        frame->height,
        outFrame->data,
        outFrame->linesize);
    if(ffmpeg::error::updateIfError(scaleCode))
        return scaleCode;

    t.stop();

    //debug("scale time: %d ms\n", t.countMsec());

    return 0;
}

int TranscodeStreamReader::decode(AVFrame * outFrame, const AVPacket* packet)
{
    int decodeCode = 0;
    int gotFrame = 0;
    while(!gotFrame && decodeCode >= 0)
        decodeCode = m_decoder->decode(outFrame, &gotFrame, packet);
    return decodeCode;
}

int TranscodeStreamReader::encode(const AVFrame * frame, AVPacket * outPacket)
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

    openCode = openVideoDecoder();
    if(openCode < 0)
        return openCode;

    int initCode = initializeScaledFrame(m_videoEncoder);
    if(initCode < 0)
        return initCode;

    auto decodedFrame = std::make_unique<ffmpeg::Frame>();
    if(!decodedFrame || !decodedFrame->frame())
        return AVERROR(ENOMEM);
    m_decodedFrame = std::move(decodedFrame);

    m_ffmpegStreamReader->addConsumer(m_consumer);

    m_state = kInitialized;
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

int TranscodeStreamReader::openVideoDecoder()
{
    auto decoder = std::make_unique<ffmpeg::Codec>();
    // todo initialize this differently for windows
    int initCode = decoder->initializeDecoder("h264_mmal");
    if (initCode < 0)
        return initCode;

    int openCode = decoder->open();
    if (openCode < 0)
        return openCode;

    debug("Selected decoder: %s\n", decoder->codec()->name);
    m_decoder = std::move(decoder);
    return 0;
}

int TranscodeStreamReader::initializeScaledFrame(const std::unique_ptr<ffmpeg::Codec>& encoder)
{
    NX_ASSERT(encoder);

    auto scaledFrame = std::make_unique<ffmpeg::Frame>();
    if (!scaledFrame || !scaledFrame->frame())
        return AVERROR(ENOMEM);
    
    AVPixelFormat encoderFormat = suggestPixelFormat(encoder);
    nxcip::Resolution resolution = m_codecContext.resolution();

    int allocCode = scaledFrame->imageAlloc(resolution.width, resolution.height, encoderFormat, 32);
    if (allocCode < 0)
        return allocCode;

    m_scaledFrame = std::move(scaledFrame);
    return 0;
}

void TranscodeStreamReader::setEncoderOptions(const std::unique_ptr<ffmpeg::Codec>& encoder)
{
    nxcip::Resolution resolution = m_codecContext.resolution();

    encoder->setFps(m_codecContext.fps());
    encoder->setResolution(resolution.width, resolution.height);
    encoder->setBitrate(m_codecContext.bitrate());
    encoder->setPixelFormat(ffmpeg::utils::suggestPixelFormat(encoder->codecID()));

    AVCodecContext* encoderContext = encoder->codecContext();
    /* don't use global header. the rpi cam doesn't stream properly with global. */
    encoderContext->flags = AV_CODEC_FLAG2_LOCAL_HEADER;
    encoderContext->global_quality = encoderContext->qmin * FF_QP2LAMBDA;
}

void TranscodeStreamReader::uninitialize()
{
    m_decoder.reset(nullptr);
    m_decodedFrame.reset(nullptr);
    m_scaledFrame.reset(nullptr);
    m_videoEncoder.reset(nullptr);
    if (m_scaleContext)
        sws_freeContext(m_scaleContext);
    m_state = kOff;
}

bool TranscodeStreamReader::ensureInitialized()
{
    switch(m_state)
    {
        case kOff:
            initialize();
            NX_DEBUG(this) << "ensureInitialized(): first initialization";
            break;
        case kModified:
            NX_DEBUG(this) << "ensureInitialized(): codec parameters modified, reinitializing";
            uninitialize();
            initialize();
            break;
    }

    return m_state == kInitialized;
}

} // namespace rpi_cam2
} // namespace nx
