#include "transcode_stream_reader.h"

#ifdef __linux__
#include <errno.h>
#endif
#include <memory>

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
#include "ffmpeg/error.h"

namespace nx {
namespace webcam_plugin {

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

    AVFrame * frame = av_frame_alloc();
    int decodeCode = m_ffmpegStreamReader->nextFrame(frame);
    if(decodeCode < 0)
        return nxcip::NX_IO_ERROR;

    AVFrame * scaledFrame = nullptr;
    int scaleCode = scale(frame, &scaledFrame);
    if(scaleCode < 0)
        return nxcip::NX_OTHER_ERROR;

    ffmpeg::Packet encodePacket;
    int encodeCode = encode(scaledFrame, encodePacket.ffmpegPacket());
    if(ffmpeg::error::updateIfError(encodeCode))
        return nxcip::NX_OTHER_ERROR;

    *lpPacket = toNxPacket(encodePacket.ffmpegPacket(), m_videoEncoder->codecID()).release();

    av_frame_free(&frame);
    av_freep(&scaledFrame->data[0]);
    av_frame_free(&scaledFrame);

    return nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::setFps( int fps )
{
    if (m_codecContext.fps() != fps)
    {
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
        m_codecContext.setResolution(resolution);
        m_modified = true;
    }
}

void TranscodeStreamReader::setBitrate(int bitrate)
{
    if(m_codecContext.bitrate() != bitrate)
    {
        m_codecContext.setBitrate(bitrate);
        m_modified = true;
    }
}

/*!
 * note! A new frame is allocated in the correct format and the contents of the input frame are 
 *       copied to it. the caller is responsible for freeing the allocated frame, but check that 
 *       it is different from the input frame before freeing both frames!
 * */
int TranscodeStreamReader::scale(AVFrame * frame, AVFrame** outFrame) const
{
    AVFrame* resultFrame = av_frame_alloc();
    if(!resultFrame)
    {
        ffmpeg::error::setLastError(AVERROR(ENOMEM));
        return ffmpeg::error::lastError();
    }

    const auto reset = 
        [&resultFrame, &outFrame](bool freep) 
        {
            if(freep)
                av_freep(&resultFrame->data[0]);
            av_frame_free(&resultFrame);
            *outFrame = nullptr;
        };

    AVCodecContext * decoderContext = m_ffmpegStreamReader->codec()->codecContext();

    const AVPixelFormat* supportedFormats = m_videoEncoder->codec()->pix_fmts;
    AVPixelFormat encoderFormat = supportedFormats
        ? ffmpeg::utils::unDeprecatePixelFormat(supportedFormats[0])
        : ffmpeg::utils::suggestPixelFormat(m_videoEncoder->codecID());

    int width = m_codecContext.resolution().width;
    int height = m_codecContext.resolution().height;

    int allocCode = av_image_alloc(
        resultFrame->data, resultFrame->linesize, width, height, encoderFormat, 32);
    if(ffmpeg::error::updateIfError(allocCode))
    {
        reset(false);
        return allocCode;
    }

    struct SwsContext * imageConvertContext = sws_getCachedContext(
        nullptr, frame->width, frame->height,
        ffmpeg::utils::unDeprecatePixelFormat(decoderContext->pix_fmt),
        width, height, encoderFormat, SWS_BICUBIC, NULL, NULL, NULL);

    int scaleCode = sws_scale(imageConvertContext, frame->data, frame->linesize, 0, frame->height,
        resultFrame->data, resultFrame->linesize);
    if(ffmpeg::error::updateIfError(scaleCode))
    {
        reset(true);
        return scaleCode;
    }

    // ffmpeg prints warnings if these fields are not explicitly set
    resultFrame->width = width;
    resultFrame->height = height;
    resultFrame->format = encoderFormat;

    sws_freeContext(imageConvertContext);

    *outFrame = resultFrame;

    return 0;
}

int TranscodeStreamReader::encode(AVFrame * frame, AVPacket * outPacket) const
{
    int encodeCode = 0;
    int gotPacket = 0;
    while(!gotPacket)
    {
        int encodeCode = m_videoEncoder->encodeVideo(outPacket, frame, &gotPacket);
        if(ffmpeg::error::updateIfError(encodeCode))
            return encodeCode;
    }
    return encodeCode;
}

int TranscodeStreamReader::initialize()
{
    int openCode = openVideoEncoder();
    if (ffmpeg::error::updateIfError(openCode))
        return openCode;

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

void TranscodeStreamReader::setEncoderOptions(const std::unique_ptr<ffmpeg::Codec>& encoder)
{
    AVCodecContext* encoderContext = encoder->codecContext();
    encoderContext->width = m_codecContext.resolution().width;
    encoderContext->height = m_codecContext.resolution().height;
    encoderContext->time_base = { 1, m_codecContext.fps() };
    encoderContext->framerate = { m_codecContext.fps(), 1 };
    encoderContext->bit_rate = m_codecContext.bitrate();
    encoderContext->pix_fmt = ffmpeg::utils::suggestPixelFormat(encoderContext->codec_id);

    /* don't use global header. the rpi cam doesn't stream properly with global. */
    // encoderContext->flags = AV_CODEC_FLAG2_LOCAL_HEADER;
    encoderContext->global_quality = encoderContext->qmin * FF_QP2LAMBDA;
}

void TranscodeStreamReader::uninitialize()
{
    m_videoEncoder.reset(nullptr);
    m_initialized = false;
}

bool TranscodeStreamReader::ensureInitialized()
{
    auto printError =
        [this]()
        {
            if(!ffmpeg::error::hasError())
                return;

#ifdef __linux__
            int err = errno;
            NX_DEBUG(this) << "ensureInitialized()::errno: " << err;
#endif
            NX_DEBUG(this) << "ensureInitialized()::m_lastAvError" << ffmpeg::error::lastError();
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

} // namespace webcam_plugin
} // namespace nx
