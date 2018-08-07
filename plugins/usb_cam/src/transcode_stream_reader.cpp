#include "transcode_stream_reader.h"

#include <stdio.h>
#include "nx/utils/app_info.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
} // extern "C"s

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "utils.h"
#include "device/utils.h"
#include "ffmpeg/stream_reader.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"

namespace nx {
namespace usb_cam {

namespace {
int constexpr RETRY_LIMIT = 10;
}

TranscodeStreamReader::TranscodeStreamReader(
    int encoderIndex,
    nxpl::TimeProvider *const timeProvider,
    const ffmpeg::CodecParameters& codecParams,
    const std::shared_ptr<ffmpeg::StreamReader>& ffmpegStreamReader)
:
    InternalStreamReader(
        encoderIndex,
        timeProvider,
        codecParams,
        ffmpegStreamReader),
        m_cameraState(kOff),
        m_retries(0),
        m_consumer(new ffmpeg::BufferedFrameConsumer(ffmpegStreamReader, codecParams)),
        m_added(false),
        m_interrupted(false)
{
}

TranscodeStreamReader::~TranscodeStreamReader()
{
    m_ffmpegStreamReader->removeFrameConsumer(m_consumer);
    uninitialize();
}

int TranscodeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    if(m_retries >= RETRY_LIMIT)
        return nxcip::NX_OTHER_ERROR;

    if (!ensureInitialized())
    {
        ++m_retries;
        return nxcip::NX_OTHER_ERROR;
    }

    ensureAdded();
    maybeDropFrames();

    int nxError = nxcip::NX_NO_ERROR;
    std::shared_ptr<ffmpeg::Frame> frame;
    int dropAmount = m_ffmpegStreamReader->fps() / m_codecParams.fps - 1;
    dropAmount = dropAmount > 0 ? dropAmount : 1;
    for(int i = 0; i < dropAmount; ++i)
    {
        /* needed to to prevent a dead lock while waiting for Frame::m_frameCount to drop to 0.
         * when frame still references a valid counted frame from the previous loop iteration,
         * the ffmpeg::StreamReader can't deinitialize, as it is waiting for the count to drop to 0.
         */
        frame = nullptr;
        frame = m_consumer->popFront();
        if(m_interrupted || !frame)
        {
            m_interrupted = false;
            return nxcip::NX_INTERRUPTED;
        }
    }

    addTimeStamp(frame->pts(), frame->timeStamp());

    scaleNextFrame(frame.get(), &nxError);
    if(nxError != nxcip::NX_NO_ERROR)
        return nxError;

    ffmpeg::Packet encodePacket(m_encoder->codecID());
    encodeNextPacket(&encodePacket, &nxError);
    if(nxError != nxcip::NX_NO_ERROR)
        return nxError;

    *lpPacket = toNxPacket(
        encodePacket.packet(),
        m_encoder->codecID(),
        getNxTimeStamp(encodePacket.pts()) * 1000,
        false).release();

    return nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::interrupt()
{
    m_interrupted = true;
    m_added = false;
    m_consumer->interrupt();
    m_consumer->clear();
    m_ffmpegStreamReader->removeFrameConsumer(m_consumer);
}

void TranscodeStreamReader::setFps(float fps)
{
    if (m_codecParams.fps != fps)
    {
        InternalStreamReader::setFps(fps);
        m_consumer->setFps(fps);
        m_cameraState = kModified;
    }
}

void TranscodeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    if (m_codecParams.width != resolution.width || m_codecParams.height != resolution.height)
    {
        InternalStreamReader::setResolution(resolution);
        m_consumer->setResolution(resolution.width, resolution.height);
        m_cameraState = kModified;
    }
}

void TranscodeStreamReader::setBitrate(int bitrate)
{
    if (m_codecParams.bitrate != bitrate)
    {
        InternalStreamReader::setBitrate(bitrate);
        m_consumer->setBitrate(bitrate);
        m_cameraState = kModified;
    }
}

/*!
 * Scale @param frame, modifying the preallocated @param outFrame whose size and format are expected
 * to have already been set.
 * */
int TranscodeStreamReader::scale(AVFrame * frame, AVFrame* outFrame)
{
    AVPixelFormat pixelFormat = frame->format != -1 
        ? (AVPixelFormat)frame->format 
        : m_ffmpegStreamReader->decoderPixelFormat();
        
    struct SwsContext * scaleContext = sws_getCachedContext(
        nullptr,
        frame->width,
        frame->height,
        ffmpeg::utils::unDeprecatePixelFormat(pixelFormat),
        m_codecParams.width,
        m_codecParams.height,
        ffmpeg::utils::unDeprecatePixelFormat(m_encoder->codec()->pix_fmts[0]),
        SWS_FAST_BILINEAR,
        nullptr,
        nullptr,
        nullptr);
    if (!scaleContext)
        return AVERROR(ENOMEM);

    int scaleCode = sws_scale(
        scaleContext,
        frame->data,
        frame->linesize,
        0,
        frame->height,
        outFrame->data,
        outFrame->linesize);

    sws_freeContext(scaleContext);

    return scaleCode;
}

int TranscodeStreamReader::encode(const ffmpeg::Frame * frame, ffmpeg::Packet * outPacket)
{
    int encodeCode = 0;
    int gotPacket = 0;
    while(!gotPacket)
    {   
        encodeCode = m_encoder->encode(outPacket->packet(), frame->frame(), &gotPacket);
        if (encodeCode < 0)
            return encodeCode;
        else if (encodeCode == 0 && !gotPacket)
            return encodeCode;
    }
    
    return encodeCode;
}

void TranscodeStreamReader::scaleNextFrame(const ffmpeg::Frame * frame, int * nxError)
{
    int scaleCode = scale(frame->frame(), m_scaledFrame->frame());
    if (scaleCode < 0)
    {
        NX_DEBUG(this) << "scale error:" << ffmpeg::utils::errorToString(scaleCode);
        *nxError = nxcip::NX_OTHER_ERROR;
        return;
    }

    av_frame_copy_props(m_scaledFrame->frame(), frame->frame());
    *nxError = nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::encodeNextPacket(ffmpeg::Packet * outPacket, int * nxError)
{
    bool gotPacket = false;
    while (!gotPacket)
    {
        int returnCode = m_encoder->sendFrame(m_scaledFrame->frame());
        bool needReceive = returnCode == 0 || returnCode == AVERROR(EAGAIN);
        while (needReceive)
        {
            returnCode = m_encoder->receivePacket(outPacket->packet());
            if (returnCode == 0 || returnCode == AVERROR(EAGAIN))
            {
                gotPacket = returnCode == 0;
                break;
            }
            else // something very bad happened
            {
                *nxError = nxcip::NX_OTHER_ERROR;
                return;
            }
        }
        if (!needReceive) // something very bad happened
        {
            *nxError = nxcip::NX_OTHER_ERROR;
            return;
        }
    }

    *nxError = nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::flush()
{
    ffmpeg::Packet packet(m_encoder->codecID());
    int returnCode = m_encoder->sendFrame(nullptr);
    while (returnCode != AVERROR_EOF)
        returnCode = m_encoder->receivePacket(packet.packet());
}

int64_t TranscodeStreamReader::getNxTimeStamp(int64_t ffmpegPts)
{
    int64_t nxTimeStamp;
    auto it = m_timeStamps.find(ffmpegPts);
    if(it != m_timeStamps.end())
    {
        nxTimeStamp = it->second;
        m_timeStamps.erase(it);
    }
    else // should never happen
    {
        nxTimeStamp = m_timeProvider->millisSinceEpoch();
    }
    return nxTimeStamp;
}

void TranscodeStreamReader::addTimeStamp(int64_t ffmpegPts, int64_t nxTimeStamp)
{
    m_timeStamps.insert(std::pair<int64_t, int64_t>(ffmpegPts, nxTimeStamp));
}

bool TranscodeStreamReader::ensureInitialized()
{
    switch(m_cameraState)
    {
        case kOff:
            initialize();
            break;
        case kModified:
            uninitialize();
            initialize();
            break;
    }

    return m_cameraState == kInitialized;
}

void TranscodeStreamReader::ensureAdded()
{
    if (!m_added)
    {
        m_added = true;
        m_ffmpegStreamReader->addFrameConsumer(m_consumer);
    }
}

int TranscodeStreamReader::initialize()
{
    int openCode = openVideoEncoder();
    if (openCode < 0)
    {
        NX_DEBUG(this) << m_ffmpegStreamReader->url() + ":" << "encoder open error:" << ffmpeg::utils::errorToString(openCode);
        m_lastFfmpegError = openCode;
        return openCode;
    }

    int initCode = initializeScaledFrame(m_encoder);
    if(initCode < 0)
    {
        NX_DEBUG(this) << m_ffmpegStreamReader->url() + ":" << "scaled frame init error:" 
            << ffmpeg::utils::errorToString(initCode);
        m_lastFfmpegError = initCode;
        return initCode;
    }

    m_cameraState = kInitialized;
    return 0;
}

int TranscodeStreamReader::openVideoEncoder()
{
    auto encoder = std::make_shared<ffmpeg::Codec>();
    int initCode = encoder->initializeEncoder("libopenh264");
    if(initCode < 0)
    {
        auto ffmpegCodecList = utils::ffmpegCodecPriorityList();
        for (const auto & codecID : ffmpegCodecList)
        {
            encoder = std::make_shared<ffmpeg::Codec>();
            initCode = encoder->initializeEncoder(codecID);
            if (initCode >= 0)
                break;
        }
    }
    if (initCode < 0)
        return initCode;

    setEncoderOptions(encoder);

    int openCode = encoder->open();
    if (openCode < 0)
        return openCode;

    m_encoder = encoder;
    return 0;
}

int TranscodeStreamReader::initializeScaledFrame(const std::shared_ptr<ffmpeg::Codec>& encoder)
{
    NX_ASSERT(encoder);

    auto scaledFrame = std::make_unique<ffmpeg::Frame>();
    if (!scaledFrame || !scaledFrame->frame())
        return AVERROR(ENOMEM);
    
    AVPixelFormat encoderFormat = 
        ffmpeg::utils::unDeprecatePixelFormat(encoder->codec()->pix_fmts[0]);

    int allocCode = 
        scaledFrame->allocateImage(m_codecParams.width, m_codecParams.height, encoderFormat, 32);
    if (allocCode < 0)
        return allocCode;

    m_scaledFrame = std::move(scaledFrame);
    return 0;
}

void TranscodeStreamReader::setEncoderOptions(const std::shared_ptr<ffmpeg::Codec>& encoder)
{
    encoder->setFps(m_codecParams.fps);
    encoder->setResolution(m_codecParams.width, m_codecParams.height);
    encoder->setBitrate(m_codecParams.bitrate);
    encoder->setPixelFormat(ffmpeg::utils::suggestPixelFormat(encoder->codecID()));
    
    AVCodecContext* encoderContext = encoder->codecContext();
    /* don't use global header. the rpi cam doesn't stream properly with global. */
    encoderContext->flags = AV_CODEC_FLAG2_LOCAL_HEADER;

    encoderContext->global_quality = encoderContext->qmin * FF_QP2LAMBDA;
}

void TranscodeStreamReader::maybeDropFrames()
{
    if (m_consumer->size() >= m_codecParams.fps)
        m_consumer->clear();
}

void TranscodeStreamReader::uninitialize()
{
    flush();
    m_scaledFrame.reset(nullptr);
    m_encoder = nullptr;
    m_cameraState = kOff;
}

} // namespace usb_cam
} // namespace nx
