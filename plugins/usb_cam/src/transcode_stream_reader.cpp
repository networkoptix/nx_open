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

namespace nx {
namespace usb_cam {

TranscodeStreamReader::TranscodeStreamReader(
    int encoderIndex,
    const CodecParameters& codecParams,
    const std::shared_ptr<Camera>& camera)
    :
    StreamReaderPrivate(
        encoderIndex,
        codecParams,
        camera),
    m_videoFrameConsumer(new BufferedVideoFrameConsumer(m_camera->videoStream(), m_codecParams))
{
    calculateTimePerFrame();
}

TranscodeStreamReader::~TranscodeStreamReader()
{
    uninitialize();
    m_videoFrameConsumer->interrupt();
    // Avoid virtual removeConsumer()
    m_camera->videoStream()->removeFrameConsumer(m_videoFrameConsumer);
}   

int TranscodeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    if (!ensureInitialized())
        return nxcip::NX_NO_DATA;

    ensureConsumerAdded();

    int outNxError = nxcip::NX_NO_ERROR;
    auto packet = nextPacket(&outNxError);

    if (outNxError != nxcip::NX_NO_ERROR)
    {
        handleIoError();
        return outNxError;
    }

    if (!packet)
    {
        handleIoError();
        return nxcip::NX_OTHER_ERROR;
    }

    *lpPacket = toNxPacket(packet.get()).release();

    return nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::interrupt()
{
    // Note: StreamReaderPrivate::interrupt calls removeConsumer(), which is overriden here.
    StreamReaderPrivate::interrupt();
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

bool TranscodeStreamReader::shouldDrop(const ffmpeg::Frame * frame)
{
    if (!frame)
        return true;
    
    // If this stream's requested framerate is equal to the cameras' requested framerate,
    // never drop.
    if(m_codecParams.fps >= m_camera->videoStream()->fps())
        return false;

    // The Mmal decoder can reorder frames, causing time stamps to be out of order. The h263 encoder
    // throws an error if the frame it encodes is equal to or earlier in time than the previous
    // frame, so drop it to avoid this error.
    if (m_lastVideoPts != AV_NOPTS_VALUE && frame->pts() <= m_lastVideoPts)
        return true;

    uint64_t now = m_camera->millisSinceEpoch();

    // If the time stamp of this frame minus the amount of time per video frame is lower
    // than the timestamp of the last transcoded frame, we should drop.
    bool drop = now - m_timePerFrame < m_lastTimestamp;
    
    if (!drop)
        m_lastTimestamp = now;

    return drop;
}

std::shared_ptr<ffmpeg::Packet> TranscodeStreamReader::transcodeVideo(
    const ffmpeg::Frame * frame,
    int * outNxError)
{
    *outNxError = nxcip::NX_NO_ERROR;

    m_lastVideoPts = frame->pts();

    int result = scale(frame->frame(), m_scaledFrame->frame());
    if (result < 0)
    {
        *outNxError = nxcip::NX_OTHER_ERROR;
        return nullptr;
    }

    m_scaledFrame->frame()->pts = frame->pts();
    m_scaledFrame->frame()->pkt_pts = frame->packetPts();

    auto packet = std::make_shared<ffmpeg::Packet>(m_encoder->codecId(), AVMEDIA_TYPE_VIDEO);    

    result = encode(m_scaledFrame.get(), packet.get());
    if (result < 0)
    {
        *outNxError = nxcip::NX_OTHER_ERROR;
        return nullptr;
    }

    // Don't allow too many dangling timestamps.
    if(m_timestamps.size() > 30)
        m_timestamps.clear();

    m_timestamps.addTimestamp(frame->packetPts(), frame->timestamp());

    uint64_t nxTimestamp = 0;
    if (!m_timestamps.getNxTimestamp(packet->pts(), &nxTimestamp, true /*eraseEntry*/))
        nxTimestamp = m_camera->millisSinceEpoch();

    packet->setTimestamp(nxTimestamp);
    return packet;
}


int TranscodeStreamReader::encode(const ffmpeg::Frame* frame, ffmpeg::Packet * outPacket)
{
    int result = m_encoder->sendFrame(frame->frame());
    if (result == 0 || result == AVERROR(EAGAIN))
        return m_encoder->receivePacket(outPacket->packet());
    return result;
}

bool TranscodeStreamReader::waitForTimeSpan(
    const std::chrono::milliseconds& timeSpan,
    const std::chrono::milliseconds& timeOut)
{
    uint64_t waitStart = m_camera->millisSinceEpoch();
    for (;;)
    {
        if (m_interrupted)
            return false;

        std::set<uint64_t> allTimeStamps;
        
        auto videoTimeStamps = m_videoFrameConsumer->timestamps();
        for (const auto & k : videoTimeStamps)
            allTimeStamps.insert(k);
        
        auto audioTimeStamps = m_avConsumer->timestamps();
        for (const auto & k : audioTimeStamps)
            allTimeStamps.insert(k);

        if (allTimeStamps.empty() 
            || *allTimeStamps.rbegin() - *allTimeStamps.begin() < timeSpan.count())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (m_camera->millisSinceEpoch() - waitStart >= timeOut.count())
                return false;
        }
        else
        {
            return true;
        }
    }
}

std::shared_ptr<ffmpeg::Packet> TranscodeStreamReader::nextPacket(int * outNxError)
{
    *outNxError = nxcip::NX_NO_ERROR;

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

    const auto ioError =
        [this, &outNxError]() -> bool
        {
            if (m_camera->ioError())
            {
                *outNxError = nxcip::NX_IO_ERROR;
                return true;
            }
            return false;
        };

    // Audio disabled
    if (!m_camera->audioEnabled())
    {
        for (;;)
        {
            auto videoFrame = m_videoFrameConsumer->popOldest(kWaitTimeOut);
            if (interrupted() || ioError())
                return nullptr;
            if (!shouldDrop(videoFrame.get()))
                return transcodeVideo(videoFrame.get(), outNxError);
        }
    }
    
    // Audio enabled
    for(;;)
    {
        if (waitForTimeSpan(m_camera->videoStream()->actualTimePerFrame(), kWaitTimeOut))
            break;
        else if (interrupted() || ioError())
            return nullptr;
    }

    static const std::chrono::milliseconds timeOut = std::chrono::milliseconds(1);

    for (;;)
    {
        auto videoFrame = m_videoFrameConsumer->popOldest(timeOut);
        if (interrupted() || ioError())
            return nullptr;
        if (!shouldDrop(videoFrame.get()))
        {
            if (auto videoPacket = transcodeVideo(videoFrame.get(), outNxError))
                m_avConsumer->givePacket(videoPacket);
        }

        // Release the reference so that internally VideoStream::m_frameCount will be decremented.
        videoFrame = nullptr;

        auto peeked = m_avConsumer->peekOldest(timeOut);
        if (!peeked)
        {
            if (interrupted() || ioError())
                return nullptr;
            continue;
        }

        auto popped =  m_avConsumer->popOldest(kWaitTimeOut);
        if(!popped)
        {
            if (interrupted() || ioError())
                return nullptr;
        }
        
        return popped;
    }
}

bool TranscodeStreamReader::ensureInitialized()
{   
    if (m_initCode < 0)
        return false;

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
    StreamReaderPrivate::ensureConsumerAdded();
    if (!m_videoConsumerAdded)
    {
        m_camera->videoStream()->addFrameConsumer(m_videoFrameConsumer);
        m_videoConsumerAdded = true;
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
    if (result < 0)
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
    for (const auto & codecId : ffmpegCodecList)
    {
        encoder = std::make_unique<ffmpeg::Codec>();
        result = encoder->initializeEncoder(codecId);
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

    context->mb_decision = FF_MB_DECISION_BITS;
    // Don't use global header. the rpi cam doesn't stream properly with global.
    context->flags = AV_CODEC_FLAG2_LOCAL_HEADER;
    context->global_quality = context->qmin * FF_QP2LAMBDA;
    context->gop_size = m_codecParams.fps;
}

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

void TranscodeStreamReader::removeVideoConsumer()
{
    m_camera->videoStream()->removeFrameConsumer(m_videoFrameConsumer);
    m_videoConsumerAdded = false;
}

void TranscodeStreamReader::handleIoError()
{
    if (m_camera->videoStream()->ioError())
        removeVideoConsumer();
    if (m_camera->audioStream()->ioError())
        removeAudioConsumer();
}

} // namespace usb_cam
} // namespace nx