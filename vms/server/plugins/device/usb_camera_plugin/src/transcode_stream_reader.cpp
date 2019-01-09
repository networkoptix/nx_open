#include "transcode_stream_reader.h"

#include <set>

extern "C" {
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
} // extern "C"

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
    m_videoFrameConsumer->interrupt();
    // Avoid virtual removeVideoConsumer()
    m_camera->videoStream()->removeFrameConsumer(m_videoFrameConsumer);
    
    uninitialize();
}

int TranscodeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    if (!ensureEncoderInitialized())
        return nxcip::NX_NO_DATA;

    ensureConsumerAdded();

    int outNxError = nxcip::NX_NO_ERROR;
    auto packet = nextPacket(&outNxError);

    if (outNxError != nxcip::NX_NO_ERROR)
    {
        removeConsumer();
        return outNxError;
    }

    if (!packet)
    {
        removeConsumer();
        return nxcip::NX_OTHER_ERROR;
    }

    *lpPacket = toNxPacket(packet.get()).release();

    return nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::interrupt()
{
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
        m_encoderNeedsReinitialization = true;
    }
}

void TranscodeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    if (m_codecParams.resolution.width != resolution.width 
        || m_codecParams.resolution.height != resolution.height)
    {
        StreamReaderPrivate::setResolution(resolution);
        m_videoFrameConsumer->setResolution(resolution);
        m_encoderNeedsReinitialization = true;
    }
}

void TranscodeStreamReader::setBitrate(int bitrate)
{
    if (m_codecParams.bitrate != bitrate)
    {
        StreamReaderPrivate::setBitrate(bitrate);
        m_videoFrameConsumer->setBitrate(bitrate);
        m_encoderNeedsReinitialization = true;
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

    // The Mmal decoder can reorder frames, causing time stamps to be out of order. 
    // The h263 encoder throws an error if the inputFrame it encodes is equal to or earlier in time 
    // than the previous inputFrame, so drop it to avoid this error.
    if (m_lastVideoPts != AV_NOPTS_VALUE && frame->pts() <= m_lastVideoPts)
        return true;

    uint64_t now = m_camera->millisSinceEpoch();

    // If the time stamp of this inputFrame minus the amount of time per video inputFrame is lower
    // than the timestamp of the last transcoded inputFrame, we should drop.
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

    int result = scale(frame->frame());
    if (result < 0)
    {
        *outNxError = nxcip::NX_OTHER_ERROR;
        return nullptr;
    }

    auto packet = std::make_shared<ffmpeg::Packet>(m_encoder->codecId(), AVMEDIA_TYPE_VIDEO);    

    result = encode(m_scaledFrame.get(), packet.get());
    if (result < 0)
    {
        *outNxError = nxcip::NX_OTHER_ERROR;
        return nullptr;
    }

    m_timestamps.addTimestamp(frame->packetPts(), frame->timestamp());

    auto nxTimestamp = m_timestamps.takeNxTimestamp(packet->pts());
    if (!nxTimestamp.has_value())
        nxTimestamp.emplace(m_camera->millisSinceEpoch());

    packet->setTimestamp(nxTimestamp.value());
    return packet;
}

int TranscodeStreamReader::encode(const ffmpeg::Frame* frame, ffmpeg::Packet * outPacket)
{
    int result = m_encoder->sendFrame(frame->frame());
    if (result == 0 || result == AVERROR(EAGAIN))
        return m_encoder->receivePacket(outPacket->packet());
    return result;
}

bool TranscodeStreamReader::waitForTimespan(
    const std::chrono::milliseconds& timespan,
    const std::chrono::milliseconds& timeout)
{
    uint64_t waitStart = m_camera->millisSinceEpoch();
    for (;;)
    {
        if (m_interrupted)
            return false;

        std::set<uint64_t> allTimestamps;

        auto videoTimeStamps = m_videoFrameConsumer->timestamps();
        for (const auto & k : videoTimeStamps)
            allTimestamps.insert(k);

        auto audioTimeStamps = m_avConsumer->timestamps();
        for (const auto & k : audioTimeStamps)
            allTimestamps.insert(k);

        if (allTimestamps.empty() 
            || *allTimestamps.rbegin() - *allTimestamps.begin() < timespan.count())
        {
            static constexpr std::chrono::milliseconds kSleep(1);
            std::this_thread::sleep_for(kSleep);
            if (m_camera->millisSinceEpoch() - waitStart >= timeout.count())
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
    while (!m_camera->audioEnabled())
    {
        auto videoFrame = m_videoFrameConsumer->popOldest(kWaitTimeout);
        if (interrupted() || ioError())
            return nullptr;
        if (!shouldDrop(videoFrame.get()))
            return transcodeVideo(videoFrame.get(), outNxError);
    }

    // Audio enabled
    for (;;)
    {
        if (waitForTimespan(kStreamDelay, kWaitTimeout))
            break;
        else if (interrupted() || ioError())
            return nullptr;
    }

    static constexpr std::chrono::milliseconds kTimeout(1);

    for (;;)
    {
        {
            // videoFrame needs to be released after this scope to prevent deadlock
            auto videoFrame = m_videoFrameConsumer->popOldest(kTimeout);
            if (interrupted() || ioError())
                return nullptr;
            if (!shouldDrop(videoFrame.get()))
            {
                if (auto videoPacket = transcodeVideo(videoFrame.get(), outNxError))
                    m_avConsumer->givePacket(videoPacket);
            }
        }

        auto peeked = m_avConsumer->peekOldest(kTimeout);
        if (!peeked)
        {
            if (interrupted() || ioError())
                return nullptr;
            continue;
        }

        auto popped =  m_avConsumer->popOldest(kTimeout);
        if(!popped)
        {
            if (interrupted() || ioError())
                return nullptr;
        }

        return popped;
    }
}

bool TranscodeStreamReader::ensureEncoderInitialized()
{
    if(!m_encoder || m_encoderNeedsReinitialization)
    {
        m_initCode = initializeVideoEncoder();
        if(m_initCode < 0)
            m_camera->setLastError(m_initCode);
    }

    return !m_encoderNeedsReinitialization;
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

void TranscodeStreamReader::uninitialize()
{
    uninitializeScaleContext();
    m_encoder.reset(nullptr);
    m_encoderNeedsReinitialization = true;
}

int TranscodeStreamReader::initializeVideoEncoder()
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
    m_encoderNeedsReinitialization = false;

    return 0;
}

int TranscodeStreamReader::initializeScaledFrame(const ffmpeg::Codec* encoder)
{
    NX_ASSERT(encoder);

    auto scaledFrame = std::make_unique<ffmpeg::Frame>();
    if (!scaledFrame || !scaledFrame->frame())
        return AVERROR(ENOMEM);

    AVPixelFormat encoderFormat = ffmpeg::utils::unDeprecatePixelFormat(
        encoder->codec()->pix_fmts[0]);

    int result = scaledFrame->getBuffer(
        encoderFormat,
        m_codecParams.resolution.width,
        m_codecParams.resolution.height, 32);

    if (result < 0)
        return result;

    m_scaledFrame = std::move(scaledFrame);
    return 0;
}

int TranscodeStreamReader::initializeScaledFrame(const PictureParameters & pictureParams)
{
    auto scaledFrame = std::make_unique<ffmpeg::Frame>();
    if (!scaledFrame || !scaledFrame->frame())
        return AVERROR(ENOMEM);

    int result = scaledFrame->getBuffer(
        pictureParams.pixelFormat,
        pictureParams.width,
        pictureParams.height);

    if (result < 0)
        return result;

    m_scaledFrame = std::move(scaledFrame);
    
    return 0;
}

void TranscodeStreamReader::setEncoderOptions(ffmpeg::Codec* encoder)
{
    encoder->setFps(m_codecParams.fps);
    encoder->setResolution(m_codecParams.resolution.width, m_codecParams.resolution.height);
    encoder->setBitrate(m_codecParams.bitrate);
    encoder->setPixelFormat(ffmpeg::utils::suggestPixelFormat(encoder->codec()));

    AVCodecContext* context = encoder->codecContext();

    context->mb_decision = FF_MB_DECISION_BITS;
    // Don't use global header. the rpi cam doesn't stream properly with global.
    context->flags = AV_CODEC_FLAG2_LOCAL_HEADER;
    context->global_quality = context->qmin * FF_QP2LAMBDA;
    context->gop_size = m_codecParams.fps;
}

int TranscodeStreamReader::initializeScaleContext(const ScaleParameters & scaleParams)
{
    m_scaleContext = sws_getContext(
        scaleParams.input.width,
        scaleParams.input.height,
        ffmpeg::utils::unDeprecatePixelFormat(scaleParams.input.pixelFormat),
        scaleParams.output.width,
        scaleParams.output.height,
        ffmpeg::utils::unDeprecatePixelFormat(scaleParams.output.pixelFormat),
        SWS_FAST_BILINEAR,
        nullptr,
        nullptr,
        nullptr);

    if (!m_scaleContext)
        return AVERROR(ENOMEM);

    return initializeScaledFrame(scaleParams.output);
}

void TranscodeStreamReader::uninitializeScaleContext()
{
    if(m_scaleContext)
        sws_freeContext(m_scaleContext);
    m_scaleContext = nullptr;
    m_scaledFrame.reset(nullptr);
}

int TranscodeStreamReader::reinitializeScaleContext(
    const TranscodeStreamReader::ScaleParameters& scaleParams)
{
    m_scaleParams = scaleParams;
    uninitializeScaleContext();
    m_initCode = initializeScaleContext(scaleParams);
    if (m_initCode < 0)
        m_camera->setLastError(m_initCode);
    return m_initCode;
}

TranscodeStreamReader::ScaleParameters
TranscodeStreamReader::getNewestScaleParameters(const AVFrame * inputFrame) const
{
    PictureParameters input(inputFrame);
    PictureParameters output(
        m_codecParams.resolution.width,
        m_codecParams.resolution.height,
        m_encoder->codec()->pix_fmts[0]);
    return ScaleParameters(input, output);
}

int TranscodeStreamReader::scale(const AVFrame * inputFrame)
{
    ScaleParameters newestScaleParams = getNewestScaleParameters(inputFrame);

    int result = 0;
    if (!m_scaleContext || newestScaleParams != m_scaleParams)
        result = reinitializeScaleContext(newestScaleParams);

    if (result < 0)
        return result;

    result = sws_scale(
        m_scaleContext,
        inputFrame->data,
        inputFrame->linesize,
        0,
        inputFrame->height,
        m_scaledFrame->frame()->data,
        m_scaledFrame->frame()->linesize);

    if(result < 0)
        return result;

    m_scaledFrame->frame()->pts = inputFrame->pts;
    m_scaledFrame->frame()->pkt_pts = inputFrame->pkt_pts;
    m_scaledFrame->frame()->pkt_dts = inputFrame->pkt_dts;

    return result;
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

} // namespace usb_cam
} // namespace nx
