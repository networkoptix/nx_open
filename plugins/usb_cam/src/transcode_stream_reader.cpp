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
        m_retries(0)
{
}

TranscodeStreamReader::~TranscodeStreamReader()
{
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
    maybeDropPackets();

    int nxError = nxcip::NX_NO_ERROR;
    decodeNextFrame(&nxError);
    if(nxError != nxcip::NX_NO_ERROR)
        return nxError;

    scaleNextFrame(&nxError);
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

void TranscodeStreamReader::setFps(int fps)
{
    if (m_codecParams.fps != fps)
    {
        InternalStreamReader::setFps(fps);
        m_cameraState = kModified;
    }
}

void TranscodeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    if (m_codecParams.width != resolution.width || m_codecParams.height != resolution.height)
    {
        InternalStreamReader::setResolution(resolution);
        m_cameraState = kModified;
    }
}

void TranscodeStreamReader::setBitrate(int bitrate)
{
    if (m_codecParams.bitrate != bitrate)
    {
        InternalStreamReader::setBitrate(bitrate);
        m_cameraState = kModified;
    }
}

/*!
 * Scale @param frame, modifying the preallocated @param outFrame whose size and format are expected
 * to have already been set.
 * */
int TranscodeStreamReader::scale(AVFrame * frame, AVFrame* outFrame)
{
    AVPixelFormat pixelFormat = 
        frame->format != -1 ? (AVPixelFormat)frame->format : m_decoder->pixelFormat();
        
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

int TranscodeStreamReader::decode(AVFrame * outFrame, AVPacket* packet, int * gotFrame)
{
    int decodeSize = 0;
    while (!(*gotFrame))
    {
        decodeSize = m_decoder->decode(outFrame, gotFrame, packet);
        if (decodeSize > 0 && !(*gotFrame))
        {
            packet->data += decodeSize;
            packet->size -= decodeSize;
            if (packet->size <= 0)
                break;
        }
        else if (decodeSize <= 0)
            break;
    }
    
    return decodeSize;
}

void TranscodeStreamReader::decodeNextFrame(int * nxError)
{
    int frameDropAmount = m_ffmpegStreamReader->fps() / m_codecParams.fps - 1;
    frameDropAmount = frameDropAmount <= 0 ? 1 : frameDropAmount;
    for (int i = 0; i < frameDropAmount; ++i)
    {
        int gotFrame = 0;
        while (!gotFrame)
        {
            auto packet = m_consumer->popFront();
            if (m_interrupted)
            {
                m_interrupted = false;
                *nxError = nxcip::NX_INTERRUPTED;
                return;
            }

            if(!packet)
            {
                *nxError = nxcip::NX_TRY_AGAIN;
                return;
            }

            addTimeStamp(packet->pts(), packet->timeStamp());

            auto copyPacket = std::make_shared<ffmpeg::Packet>(packet->codecID());
            packet->copy(copyPacket.get());

            m_decodedFrame->unreference();
            int decodeCode = decode(m_decodedFrame->frame(), copyPacket->packet(), &gotFrame);
            if (decodeCode < 0)
            {
                NX_DEBUG(this) << ffmpeg::utils::errorToString(decodeCode);
                if (i == frameDropAmount - 1)
                {
                    *nxError = nxcip::NX_TRY_AGAIN;
                    return;
                }
            }
        }
    }

    *nxError = nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::scaleNextFrame(int * nxError)
{
    int scaleCode = scale(m_decodedFrame->frame(), m_scaledFrame->frame());
    if (scaleCode < 0)
    {
        NX_DEBUG(this) << "scale error:" << ffmpeg::utils::errorToString(scaleCode);
        *nxError = nxcip::NX_OTHER_ERROR;
        return;
    }

    m_scaledFrame->frame()->pts = m_decodedFrame->pts();
    *nxError = nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::encodeNextPacket(ffmpeg::Packet * outPacket, int * nxError)
{
    int encodeCode = encode(m_scaledFrame.get(), outPacket);
    if(encodeCode < 0)
    {
        NX_DEBUG(this) << "encode error:" << ffmpeg::utils::errorToString(encodeCode);
        *nxError = nxcip::NX_TRY_AGAIN;
        return;
    }

    *nxError = nxcip::NX_NO_ERROR;
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

int TranscodeStreamReader::initialize()
{
    int openCode = openVideoDecoder();
    if(openCode < 0)
    {
        NX_DEBUG(this) << m_ffmpegStreamReader->url() + ":" << "decoder open error:" << ffmpeg::utils::errorToString(openCode);
        m_lastFfmpegError = openCode;
        return openCode;
    }

    openCode = openVideoEncoder();
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

    auto decodedFrame = std::make_unique<ffmpeg::Frame>();
    if(!decodedFrame || !decodedFrame->frame())
    {
        m_lastFfmpegError = AVERROR(ENOMEM);
        NX_DEBUG(this) 
            << m_ffmpegStreamReader->url() + ":" << "decoded frame init error:" 
            << ffmpeg::utils::errorToString(m_lastFfmpegError);
        return AVERROR(ENOMEM);
    }
    m_decodedFrame = std::move(decodedFrame);

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

int TranscodeStreamReader::openVideoDecoder()
{
    auto decoder = std::make_unique<ffmpeg::Codec>();
    int initCode;
    if (nx::utils::AppInfo::isRaspberryPi() && m_codecParams.codecID == AV_CODEC_ID_H264)
        initCode = decoder->initializeDecoder("h264_mmal");
    else
        initCode = m_ffmpegStreamReader->initializeDecoderFromStream(decoder.get());
    if (initCode < 0)
        return initCode;

    int openCode = decoder->open();
    if (openCode < 0)
        return openCode;

    m_decoder = std::move(decoder);
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

void TranscodeStreamReader::uninitialize()
{
    m_decoder.reset(nullptr);
    m_decodedFrame.reset(nullptr);
    m_scaledFrame.reset(nullptr);
    m_encoder = nullptr;
    m_cameraState = kOff;
}

} // namespace usb_cam
} // namespace nx
