#include "transcode_stream_reader.h"

#include <stdio.h>

extern "C" {
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
} // extern "C"

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "device/utils.h"
#include "ffmpeg/stream_reader.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"

namespace nx {
namespace usb_cam {

TranscodeStreamReader::TranscodeStreamReader(
    int encoderIndex,
    nxpt::CommonRefManager* const parentRefManager,
    nxpl::TimeProvider *const timeProvider,
    const nxcip::CameraInfo& cameraInfo,
    const ffmpeg::CodecParameters& codecParams,
    const std::shared_ptr<ffmpeg::StreamReader>& ffmpegStreamReader)
:
    StreamReader(
        encoderIndex,
        parentRefManager,
        timeProvider,
        cameraInfo,
        codecParams,
        ffmpegStreamReader),
        m_state(kOff)
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
        getNxTimeStamp(encodePacket.pts()) * 1000).release();

    return nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::setFps(int fps)
{
    if (m_codecParams.fps != fps)
    {
        StreamReader::setFps(fps);
        m_state = kModified;
    }
}

void TranscodeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    if (m_codecParams.width != resolution.width || m_codecParams.height != resolution.height)
    {
        StreamReader::setResolution(resolution);
        m_state = kModified;
    }
}

void TranscodeStreamReader::setBitrate(int bitrate)
{
    if (m_codecParams.bitrate != bitrate)
    {
        StreamReader::setBitrate(bitrate);
        m_state = kModified;
    }
}

int TranscodeStreamReader::lastFfmpegError() const
{
    return m_lastFfmpegError;
}

/*!
 * Scale @param frame, modifying the preallocated @param outFrame whose size and format are expected
 * to have already been set.
 * */
int TranscodeStreamReader::scale(AVFrame * frame, AVFrame* outFrame)
{
    struct SwsContext * scaleContext = sws_getCachedContext(
        nullptr,
        frame->width,
        frame->height,
        m_decoder->pixelFormat(),
        m_codecParams.width,
        m_codecParams.height,
        m_encoder->codec()->pix_fmts[0],
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

int TranscodeStreamReader::decode(AVFrame * outFrame, const AVPacket* packet, int * gotFrame)
{
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = packet->data;
    pkt.size = packet->size;
    pkt.pts = packet->pts;
    pkt.dts = packet->dts;

    int decodeSize = 0;
    while (!(*gotFrame))
    {
        decodeSize = m_decoder->decode(outFrame, gotFrame, &pkt);
        if (decodeSize > 0 && !(*gotFrame))
        {
            pkt.data += decodeSize;
            pkt.size -= decodeSize;
            if (pkt.size == 0)
                break;
        }
        else if (decodeSize <= 0)
            break;
    }
    
    return decodeSize;
}

void TranscodeStreamReader::decodeNextFrame(int * nxError)
{
    std::shared_ptr<ffmpeg::Packet> packet;
    int frameDropAmount = m_ffmpegStreamReader->fps() / m_codecParams.fps - 1;
    frameDropAmount = frameDropAmount <= 0 ? 1 : frameDropAmount;
    for (int i = 0; i < frameDropAmount; ++i)
    {
        int gotFrame = 0;
        while (!gotFrame)
        {
            packet = nextPacket();
            if(m_interrupted)
            {
                m_interrupted = false;
                *nxError = nxcip::NX_NO_DATA;
                return;
            }
                
            addTimeStamp(packet->pts(), packet->timeStamp());
            m_decodedFrame->unreference();
            int decodeCode = decode(m_decodedFrame->frame(), packet->packet(), &gotFrame);
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

int TranscodeStreamReader::initialize()
{
    m_ffmpegStreamReader->addConsumer(m_consumer);

    int openCode = openVideoDecoder();
    if(openCode < 0)
    {
        NX_DEBUG(this) << "decoder open error:" << ffmpeg::utils::errorToString(openCode);
        m_lastFfmpegError = openCode;
        return openCode;
    }

    openCode = openVideoEncoder();
    if (openCode < 0)
    {
        NX_DEBUG(this) << "encoder open error:" << ffmpeg::utils::errorToString(openCode);
        m_lastFfmpegError = openCode;
        return openCode;
    }

    int initCode = initializeScaledFrame(m_encoder);
    if(initCode < 0)
    {
        NX_DEBUG(this) << "scaled frame init error:" << ffmpeg::utils::errorToString(initCode);
        m_lastFfmpegError = initCode;
        return initCode;
    }

    auto decodedFrame = std::make_unique<ffmpeg::Frame>();
    if(!decodedFrame || !decodedFrame->frame())
    {
        m_lastFfmpegError = AVERROR(ENOMEM);
        NX_DEBUG(this) << "decoded frame init error:" << ffmpeg::utils::errorToString(m_lastFfmpegError);
        return AVERROR(ENOMEM);
    }
    m_decodedFrame = std::move(decodedFrame);

    m_state = kInitialized;
    return 0;
}

int TranscodeStreamReader::openVideoEncoder()
{
    auto encoder = std::make_shared<ffmpeg::Codec>();
   
    //todo initialize the right decoder for windows
    int initCode = encoder->initializeEncoder("libopenh264");
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
    // todo initialize this differently for windows
    int initCode = decoder->initializeDecoder("h264_mmal");
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
    
    AVPixelFormat encoderFormat = encoder->codec()->pix_fmts[0];

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

    // if(encoder->codec()->capabilities | AV_CODEC_CAP_AUTO_THREADS)
    //     encoderContext->thread_count = 0;

    encoderContext->global_quality = encoderContext->qmin * FF_QP2LAMBDA;
}

void TranscodeStreamReader::uninitialize()
{
    m_ffmpegStreamReader->removeConsumer(m_consumer);
    m_decoder.reset(nullptr);
    m_decodedFrame.reset(nullptr);
    m_scaledFrame.reset(nullptr);
    m_encoder = nullptr;
    m_state = kOff;
}

} // namespace usb_cam
} // namespace nx
