#include "transcode_stream_reader.h"

#include <stdio.h>
#include <set>

extern "C" {
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
} // extern "C"

#include <nx/utils/log/log.h>
#include "nx/utils/app_info.h"

#include "device/utils.h"
#include "ffmpeg/utils.h"
#include "utils.h"

namespace nx {
namespace usb_cam {

namespace {

int constexpr kRetryLimit = 10;
int constexpr kMsecInSec = 1000;

uint64_t earlier = 0;
uint64_t lastTs = 0;

}

TranscodeStreamReader::TranscodeStreamReader(
    int encoderIndex,
    const CodecParameters& codecParams,
    const std::shared_ptr<Camera>& camera)
    :
    StreamReaderPrivate(
        encoderIndex,
        codecParams,
        camera),
    m_cameraState(csOff),
    m_retries(0),
    m_initCode(0),
    m_lastVideoTimeStamp(0),
    m_videoFrameConsumer(new BufferedVideoFrameConsumer(m_camera->videoStream(), m_codecParams))
{
    std::cout << "TranscodeStreamReader" << std::endl;
    calculateTimePerFrame();
}


TranscodeStreamReader::~TranscodeStreamReader()
{
    interrupt();
    uninitialize();
    std::cout << "~TranscodeStreamReader" << std::endl;
}

int TranscodeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

   if (m_retries >= kRetryLimit)
        return nxcip::NX_NO_DATA;

    if (!ensureInitialized())
    {
        ++m_retries;
        return nxcip::NX_OTHER_ERROR;
    }

    ensureConsumerAdded();
    maybeDrop();

    int outNxError = nxcip::NX_NO_ERROR;
    auto packet = nextPacket(&outNxError);

    if(outNxError != nxcip::NX_NO_ERROR)
        return outNxError;

    if(!packet)
        return nxcip::NX_OTHER_ERROR;

    auto now = m_camera->millisSinceEpoch();
    std::stringstream ss;
    ss << packet->timeStamp()
        << ", " << now - earlier
        << ", " << packet->timeStamp() - m_lastTs
        << ", " << ffmpeg::utils::codecIDToName(packet->codecID());
    std::cout << ss.str() << std::endl;
    earlier = now;

    *lpPacket = toNxPacket(packet.get()).release();

    return nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::interrupt()
{
    StreamReaderPrivate::interrupt();
    m_camera->videoStream()->removeFrameConsumer(m_videoFrameConsumer);
    m_videoFrameConsumer->interrupt();
    m_videoFrameConsumer->flush();
}

void TranscodeStreamReader::setFps(float fps)
{
    if (m_codecParams.fps != fps)
    {
        StreamReaderPrivate::setFps(fps);
        m_videoFrameConsumer->setFps(fps);
        calculateTimePerFrame();
        m_cameraState = csModified;
    }
}

void TranscodeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    if (m_codecParams.width != resolution.width || m_codecParams.height != resolution.height)
    {
        StreamReaderPrivate::setResolution(resolution);
        m_videoFrameConsumer->setResolution(resolution.width, resolution.height);
        m_cameraState = csModified;
    }
}

void TranscodeStreamReader::setBitrate(int bitrate)
{
    if (m_codecParams.bitrate != bitrate)
    {
        StreamReaderPrivate::setBitrate(bitrate);
        m_videoFrameConsumer->setBitrate(bitrate);
        m_cameraState = csModified;
    }
}

bool TranscodeStreamReader::shouldDrop(const ffmpeg::Frame * frame) const
{
    return !frame
        || (m_codecParams.fps < m_camera->videoStream()->fps()
        && frame->timeStamp() - m_timePerFrame < m_lastVideoTimeStamp);
}

std::shared_ptr<ffmpeg::Packet> TranscodeStreamReader::transcodeVideo(const ffmpeg::Frame * frame, int * outNxError)
{
    *outNxError = nxcip::NX_NO_ERROR;

    m_timeStamps.addTimeStamp(frame->pts(), frame->timeStamp());

    int result = scale(frame->frame(), m_scaledFrame->frame());
    if (result < 0)
    {
        *outNxError = nxcip::NX_OTHER_ERROR;
        return nullptr;
    }

    m_scaledFrame->frame()->pts = frame->pts();

    auto packet = std::make_shared<ffmpeg::Packet>(m_encoder->codecID(), AVMEDIA_TYPE_VIDEO);
    
    result = encode(m_scaledFrame.get(), packet.get());
    if (result < 0)
    {
        *outNxError = nxcip::NX_OTHER_ERROR;
        return nullptr;
    }
    
    uint64_t nxTimeStamp;
    if (!m_timeStamps.getNxTimeStamp(packet->pts(), &nxTimeStamp, true /*eraseEntry*/))
        nxTimeStamp = m_camera->millisSinceEpoch();
    packet->setTimeStamp(nxTimeStamp);

    m_lastVideoTimeStamp = frame->timeStamp();

    return packet;
}


int TranscodeStreamReader::encode(const ffmpeg::Frame* frame, ffmpeg::Packet * outPacket)
{
    int result = m_encoder->sendFrame(frame->frame());
    if (result == 0 || result == AVERROR(EAGAIN))
        return m_encoder->receivePacket(outPacket->packet());
    return result;
}

void TranscodeStreamReader::waitForTimeSpan(uint64_t msecDifference)
{
    for (;;)
    {
        if (m_interrupted)
            return;

        std::set<uint64_t> allKeys;
        
        auto videoKeys = m_videoFrameConsumer->keys();
        for (const auto & k : videoKeys)
            allKeys.insert(k);
        
        auto audioKeys = m_avConsumer->keys();
        for (const auto & k : audioKeys)
            allKeys.insert(k);

        if (allKeys.empty() || *allKeys.rbegin() - *allKeys.begin() < msecDifference)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        else
            return;
    }
}

std::shared_ptr<ffmpeg::Packet> TranscodeStreamReader::nextPacket(int * outNxError)
{
    *outNxError = nxcip::NX_NO_ERROR;

    const auto otherError = 
        [this, &outNxError]() -> std::shared_ptr<ffmpeg::Packet>
        {
            *outNxError = nxcip::NX_OTHER_ERROR;
            return nullptr;
        };

    const auto interrupted = 
        [this, &outNxError]() -> bool
        {
            if (m_interrupted)
            {
                m_interrupted = false;
                *outNxError = nxcip::NX_INTERRUPTED;
                return true;
            }
            return false;
        };

    if (!m_camera->audioEnabled())
    {
        for (;;)
        {
            auto videoFrame = m_videoFrameConsumer->popFront();
            if (interrupted())
                return nullptr;
            if (!shouldDrop(videoFrame.get()))
                return transcodeVideo(videoFrame.get(), outNxError);
        }
    }
    
    // audio enabled

    uint64_t msecPerFrame = utils::msecPerFrame(m_camera->videoStream()->fps());
    waitForTimeSpan(msecPerFrame);
    if (interrupted())
        return nullptr;

    for (;;)
    {
        auto videoFrame = m_videoFrameConsumer->popFront(1);
        if (interrupted())
            return nullptr;
        if (!shouldDrop(videoFrame.get()))
        {
            if (auto videoPacket = transcodeVideo(videoFrame.get(), outNxError))
                m_avConsumer->givePacket(videoPacket);
            else
                return nullptr;
        }

        auto packet = m_avConsumer->popFront(1);
        if (interrupted())
            return nullptr;
        if (packet)
            return packet;
    }
}

bool TranscodeStreamReader::ensureInitialized()
{   
    bool needInit = m_cameraState == csOff || m_cameraState == csModified;
    if (m_cameraState == csModified)
        uninitialize();

    if(needInit)
    {
        m_initCode = initialize();
        if(m_initCode < 0)
            m_camera->setLastError(m_initCode);
    }

    return m_cameraState == csInitialized;
}

void TranscodeStreamReader::ensureConsumerAdded()
{
    if (!m_consumerAdded)
    {
        std::cout << "ENSURE CONSUMER ADDED" << std::endl;
        StreamReaderPrivate::ensureConsumerAdded();
        m_camera->videoStream()->addFrameConsumer(m_videoFrameConsumer);
    }
}

int TranscodeStreamReader::initialize()
{
    int result = openVideoEncoder();
    if (result < 0)
    {
        NX_DEBUG(this) << m_camera->videoStream()->url() + ":" 
            << "encoder open error:" << ffmpeg::utils::errorToString(result);
        return result;
    }

    result = initializeScaledFrame(m_encoder.get());
    if(result < 0)
    {
        NX_DEBUG(this) << m_camera->videoStream()->url() + ":" 
            << "scaled frame init error:" << ffmpeg::utils::errorToString(result);
        return result;
    }

    m_cameraState = csInitialized;
    return 0;
}

void TranscodeStreamReader::uninitialize()
{
    m_scaledFrame.reset(nullptr);
    if(m_encoder)
        m_encoder->flush();
    m_encoder.reset(nullptr);
    m_cameraState = csOff;
}

int TranscodeStreamReader::openVideoEncoder()
{
    int result = 0;
    std::unique_ptr<ffmpeg::Codec> encoder;
    auto ffmpegCodecList = m_camera->ffmpegCodecPriorityList();
    for (const auto & codecID : ffmpegCodecList)
    {
        encoder = std::make_unique<ffmpeg::Codec>();
        result = encoder->initializeEncoder(codecID);
        if (result >= 0)
            break;
    }
    if (result < 0)
        return result;

    setEncoderOptions(encoder.get());

    result = encoder->open();
    if (result < 0)
        return result;

    m_encoder = std::move(encoder);
    return 0;
}

int TranscodeStreamReader::initializeScaledFrame(const ffmpeg::Codec* encoder)
{
    NX_ASSERT(encoder);

    auto scaledFrame = std::make_unique<ffmpeg::Frame>();
    if (!scaledFrame || !scaledFrame->frame())
        return AVERROR(ENOMEM);
    
    AVPixelFormat encoderFormat = 
        ffmpeg::utils::unDeprecatePixelFormat(encoder->codec()->pix_fmts[0]);

    int result = 
        scaledFrame->getBuffer(encoderFormat, m_codecParams.width, m_codecParams.height, 32);
    
    if (result < 0)
        return result;

    m_scaledFrame = std::move(scaledFrame);
    return 0;
}

void TranscodeStreamReader::setEncoderOptions(ffmpeg::Codec* encoder)
{
    encoder->setFps(m_codecParams.fps);
    encoder->setResolution(m_codecParams.width, m_codecParams.height);
    encoder->setBitrate(m_codecParams.bitrate);
    encoder->setPixelFormat(ffmpeg::utils::suggestPixelFormat(encoder->codec()));
    
    AVCodecContext* context = encoder->codecContext();
    /* don't use global header. the rpi cam doesn't stream properly with global. */
    context->flags = AV_CODEC_FLAG2_LOCAL_HEADER;

    context->global_quality = context->qmin * FF_QP2LAMBDA;

    context->gop_size = m_codecParams.fps;
}

void TranscodeStreamReader::maybeDrop()
{
    float fps = m_camera->videoStream()->fps();
    if (fps == 0)
        fps = 30;
    if (m_videoFrameConsumer->size() >= m_camera->videoStream()->fps() / 2)
        m_videoFrameConsumer->flush();

    if (m_avConsumer->timeSpan() > 500)
        m_avConsumer->flush();
}

/*!
 * Scale @param frame, modifying the preallocated @param outFrame whose size and format are
 * expected to have already been set.
 */
int TranscodeStreamReader::scale(const AVFrame * frame, AVFrame* outFrame)
{
    AVPixelFormat pixelFormat = frame->format != -1 
        ? (AVPixelFormat)frame->format 
        : m_camera->videoStream()->decoderPixelFormat();
        
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

void TranscodeStreamReader::calculateTimePerFrame()
{
    m_timePerFrame = 1.0 / m_codecParams.fps * kMsecInSec;
}

int TranscodeStreamReader::framesToDrop()
{
    int audioCorrect = m_camera->audioEnabled() ? 1 : 0;
    int drop = (m_camera->videoStream()->fps() / m_codecParams.fps - 1);// - audioCorrect;
    return drop < 0 ? 0 : drop;
}


} // namespace usb_cam
} // namespace nx
