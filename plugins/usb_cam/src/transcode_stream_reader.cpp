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

#include "device/utils.h"
#include "ffmpeg/utils.h"
#include "utils.h"

namespace nx {
namespace usb_cam {

namespace {

int constexpr kRetryLimit = 10;
int constexpr kMsecInSec = 1000;

int64_t abs(int64_t value) 
{
    return value < 0 ? -value : value;
}

uint64_t lastTs = 0;
uint64_t earlier = 0;

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
    m_consumer(new BufferedVideoFrameConsumer(camera->videoStream(), codecParams)),
    m_needFramesDropped(false)
{
    std::cout << "TranscodeStreamReader" << std::endl;
    calculateTimePerFrame();
}


TranscodeStreamReader::~TranscodeStreamReader()
{
    m_camera->videoStream()->removeFrameConsumer(m_consumer);
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
    dropIfBuffersTooFull();

    int outNxError = nxcip::NX_NO_ERROR;
    nxcip::DataPacketType mediaType = nxcip::dptEmpty;

    auto packet = nextPacket(&mediaType, &outNxError);

    if(outNxError != nxcip::NX_NO_ERROR)
        return outNxError;

    if(!packet)
        return nxcip::NX_OTHER_ERROR;

    uint64_t now = m_camera->millisSinceEpoch();
    bool less = packet->timeStamp() < lastTs;

    std::string media = mediaType == nxcip::dptAudio ?  "audio" : mediaType == nxcip::dptVideo ? "video" : "none";

    //std::stringstream ss;
    //if(packet)
    //    ss << packet->timeStamp()
    //        << ", " << ffmpeg::utils::codecIDToName(packet->codecID()) 
    //        << ", " << packet->timeStamp() - lastTs
    //        << ", " << now - earlier
    //        << ", " << less;
    //else
    //    ss << "no packet";

    //std::cout << ss.str() << std::endl;

    earlier = now;
    lastTs = packet->timeStamp();

    *lpPacket = toNxPacket(
        packet.get(),
        mediaType).release();

    //std::cout << (*lpPacket)->timestamp() << std::endl;

    return nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::interrupt()
{
    StreamReaderPrivate::interrupt();
    m_camera->videoStream()->removeFrameConsumer(m_consumer);
    m_consumer->interrupt();
    m_consumer->flush();
}

void TranscodeStreamReader::setFps(float fps)
{
    if (m_codecParams.fps != fps)
    {
        StreamReaderPrivate::setFps(fps);
        m_consumer->setFps(fps);
        calculateTimePerFrame();
        m_cameraState = csModified;
    }
}

void TranscodeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    if (m_codecParams.width != resolution.width || m_codecParams.height != resolution.height)
    {
        StreamReaderPrivate::setResolution(resolution);
        m_consumer->setResolution(resolution.width, resolution.height);
        m_cameraState = csModified;
    }
}

void TranscodeStreamReader::setBitrate(int bitrate)
{
    if (m_codecParams.bitrate != bitrate)
    {
        StreamReaderPrivate::setBitrate(bitrate);
        m_consumer->setBitrate(bitrate);
        m_cameraState = csModified;
    }
}

std::shared_ptr<ffmpeg::Packet> TranscodeStreamReader::transcodeVideo(const ffmpeg::Frame * frame, int * outNxError)
{
    *outNxError = nxcip::NX_NO_ERROR;

    addTimeStamp(frame->pts(), frame->timeStamp());

    scaleNextFrame(frame, outNxError);
    if(*outNxError != nxcip::NX_NO_ERROR)
        return nullptr;

    std::shared_ptr<ffmpeg::Packet> packet = 
        std::make_shared<ffmpeg::Packet>(m_encoder->codecID());
    
    encode(packet.get(), outNxError);
    if(*outNxError != nxcip::NX_NO_ERROR)
        return nullptr;

    packet->setTimeStamp(getNxTimeStamp(packet->pts()));
    m_lastVideoTimeStamp = frame->timeStamp();

    return packet;
}

void TranscodeStreamReader::scaleNextFrame(const ffmpeg::Frame * frame, int * outNxError)
{
    int scaleCode = scale(frame->frame(), m_scaledFrame->frame());
    if (scaleCode < 0)
    {
        NX_DEBUG(this) << "scale error:" << ffmpeg::utils::errorToString(scaleCode);
        *outNxError = nxcip::NX_OTHER_ERROR;
        return;
    }

    m_scaledFrame->frame()->pts = frame->pts();
    m_scaledFrame->setTimeStamp(frame->timeStamp());
    *outNxError = nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::encode(ffmpeg::Packet * outPacket, int * outNxError)
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
                *outNxError = nxcip::NX_OTHER_ERROR;
                return;
            }
        }
        if (!needReceive) // something very bad happened
        {
            *outNxError = nxcip::NX_OTHER_ERROR;
            return;
        }
    }

    *outNxError = nxcip::NX_NO_ERROR;
}

std::shared_ptr<ffmpeg::Packet> TranscodeStreamReader::nextPacket(nxcip::DataPacketType * outMediaType, int * outNxError)
{
    const auto otherError = 
        [this, &outNxError, &outMediaType]() -> std::shared_ptr<ffmpeg::Packet>
        {
            *outMediaType = nxcip::dptEmpty;
            *outNxError = nxcip::NX_OTHER_ERROR;
            return nullptr;
        };

    const auto interrupted = 
        [this, &outNxError, &outMediaType]() -> bool
        {
            if (m_interrupted)
            {
                m_interrupted = false;
                *outMediaType = nxcip::dptEmpty;
                *outNxError = nxcip::NX_INTERRUPTED;
                return true;
            }
            return false;
        };

    #define checkPointer(ptr) \
        do { \
            if(interrupted()) \
                return nullptr; \
            if(!ptr) \
                return otherError(); \
        }while(0)

    const auto getAudio = 
        [this, &interrupted, &otherError, &outMediaType]() -> std::shared_ptr<ffmpeg::Packet>
        {
            auto packet = m_audioConsumer->popFront();
            checkPointer(packet);
            *outMediaType = nxcip::dptAudio;
            return packet;
        };

    const auto getVideo = 
        [this, &outMediaType, &outNxError](
            const ffmpeg::Frame * frame) -> std::shared_ptr<ffmpeg::Packet>
        {
            m_frameDropCount = 0;
            *outMediaType = nxcip::dptVideo;
            return transcodeVideo(frame, outNxError);
        };

    *outNxError = nxcip::NX_NO_ERROR;
    *outMediaType = nxcip::dptEmpty;

    if (!m_camera->audioEnabled())
    {
        std::shared_ptr<ffmpeg::Frame> video;
        do 
        {
            video = m_consumer->popFront();
            checkPointer(video);
        }
        while(video->timeStamp() - m_timePerFrame < m_lastVideoTimeStamp);
        return getVideo(video.get());
    }

    for(;;)
    {
        auto videoFrame = m_consumer->peekFront(true /*wait*/);
        checkPointer(videoFrame);
        auto audioPacket = m_audioConsumer->peekFront(true /*wait*/);
        checkPointer(audioPacket);
        
        //bool audioLess = audioPacket->timeStamp() < videoFrame->timeStamp();
        //std::cout << audioPacket->timeStamp() << ", " << videoFrame->timeStamp() << ", " << audioLess << std::endl;

        if(audioPacket->timeStamp() < videoFrame->timeStamp())
            return getAudio();

        videoFrame = m_consumer->popFront();
        if(videoFrame->timeStamp() - m_timePerFrame >= m_lastVideoTimeStamp)
            return getVideo(videoFrame.get());
    }
}

void TranscodeStreamReader::addTimeStamp(int64_t ffmpegPts, int64_t nxTimeStamp)
{
    m_timeStamps.addTimeStamp(ffmpegPts, nxTimeStamp);
}

int64_t TranscodeStreamReader::getNxTimeStamp(int64_t ffmpegPts)
{
    int64_t nxTimeStamp;
    if (!m_timeStamps.getNxTimeStamp(ffmpegPts, &nxTimeStamp, true/*eraseEntry*/))
    {
        std::cout << "transcoded video missing timestamp" << std::endl;
        nxTimeStamp = m_camera->millisSinceEpoch();
    }

    return nxTimeStamp;
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
        m_camera->videoStream()->addFrameConsumer(m_consumer);
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
    auto ffmpegCodecList = utils::ffmpegCodecPriorityList();
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
    encoder->setPixelFormat(ffmpeg::utils::suggestPixelFormat(encoder->codecID()));
    
    AVCodecContext* context = encoder->codecContext();
    /* don't use global header. the rpi cam doesn't stream properly with global. */
    context->flags = AV_CODEC_FLAG2_LOCAL_HEADER;

    context->global_quality = context->qmin * FF_QP2LAMBDA;

    context->gop_size = m_codecParams.fps;
}

void TranscodeStreamReader::dropIfBuffersTooFull()
{
    if (m_consumer->size() >= m_camera->videoStream()->fps())
        m_consumer->flush();

    if(m_audioConsumer->size() >= 50)
        m_audioConsumer->flush();
}

/*!
 * Scale @param frame, modifying the preallocated @param outFrame whose size and format are expected
 * to have already been set.
 * */
int TranscodeStreamReader::scale(AVFrame * frame, AVFrame* outFrame)
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
    m_timePerFrame = 1.0 / (m_codecParams.fps) * kMsecInSec;
}

int TranscodeStreamReader::framesToDrop()
{
    int audioCorrect = m_camera->audioEnabled() ? 1 : 0;
    int drop = (m_camera->videoStream()->fps() / m_codecParams.fps - 1);// - audioCorrect;
    return drop < 0 ? 0 : drop;
}


} // namespace usb_cam
} // namespace nx
