#include "transcode_stream_reader.h"

#include <set>

extern "C" {
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
} // extern "C"

#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>

#include "ffmpeg/utils.h"

namespace nx {
namespace usb_cam {

TranscodeStreamReader::TranscodeStreamReader(
    int encoderIndex,
    const CodecParameters& codecParams,
    const std::shared_ptr<Camera>& camera)
    :
    StreamReaderPrivate(encoderIndex, camera),
    m_codecParams(codecParams)
{
    calculateTimePerFrame();
}

TranscodeStreamReader::~TranscodeStreamReader()
{
    uninitialize();
}

int TranscodeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    if (!ensureEncoderInitialized())
        return nxcip::NX_NO_DATA;

    ensureConsumerAdded();

    auto packet = nextPacket();

    if (!packet)
        return handleNxError();

    *lpPacket = toNxPacket(packet.get()).release();

    return nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::setFps(float fps)
{
    if (m_codecParams.fps != fps)
    {
        m_codecParams.fps = fps;
        calculateTimePerFrame();
        m_encoderNeedsReinitialization = true;
    }
}

void TranscodeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    if (m_codecParams.resolution.width != resolution.width ||
        m_codecParams.resolution.height != resolution.height)
    {
        m_codecParams.resolution = resolution;
        m_encoderNeedsReinitialization = true;
    }
}

void TranscodeStreamReader::setBitrate(int bitrate)
{
    if (m_codecParams.bitrate != bitrate)
    {
        m_codecParams.bitrate = bitrate;
        m_encoderNeedsReinitialization = true;
    }
}

bool TranscodeStreamReader::shouldDrop(const ffmpeg::Frame* frame)
{
    if (!frame)
        return true;

    // If this stream's requested framerate is equal to the cameras' requested framerate,
    // never drop.
    if(m_codecParams.fps >= m_camera->videoStream()->fps())
        return false;

    bool drop = int64_t(frame->timestamp()) - m_lastTimestamp < m_timePerFrame;
    if (!drop)
        m_lastTimestamp = frame->timestamp();

    return drop;
}

std::shared_ptr<ffmpeg::Packet> TranscodeStreamReader::transcodeVideo(
    const ffmpeg::Packet* sourcePacket)
{
    auto frame = decode(sourcePacket);
    if (shouldDrop(frame.get()))
        return nullptr;

    int result = scale(frame->frame());
    if (result < 0)
        return nullptr;

    auto packet = std::make_shared<ffmpeg::Packet>(m_encoder->codecId(), AVMEDIA_TYPE_VIDEO);

    result = encode(m_scaledFrame.get(), packet.get());
    if (result < 0)
        return nullptr;

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

std::shared_ptr<ffmpeg::Packet> TranscodeStreamReader::nextPacket()
{
    for (;;)
    {
        auto popped =  m_avConsumer->pop();
        if (!popped)
        {
            if (shouldStopWaitingForData())
                return nullptr;
            continue;
        }
        if (popped->mediaType() == AVMEDIA_TYPE_VIDEO)
        {
            if (!m_decoder)
                initializeDecoder();
            if (auto videoPacket = transcodeVideo(popped.get()))
                return videoPacket;
        }
        else
        {
            return popped;
        }
    }
    return nullptr;
}

bool TranscodeStreamReader::ensureEncoderInitialized()
{
    if (!m_encoder || m_encoderNeedsReinitialization)
    {
        m_initCode = initializeVideoEncoder();
        if (m_initCode < 0)
            m_camera->setLastError(m_initCode);
    }

    return !m_encoderNeedsReinitialization;
}

void TranscodeStreamReader::uninitialize()
{
    uninitializeScaleContext();
    if (m_decoder)
        m_decoder->flush();
    m_decoder.reset(nullptr);
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

TranscodeStreamReader::ScaleParameters TranscodeStreamReader::getNewestScaleParameters(
    const AVFrame * inputFrame) const
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

int TranscodeStreamReader::initializeDecoder()
{
    auto decoder = std::make_unique<ffmpeg::Codec>();

    int status;
    AVCodecParameters* codecPar = m_camera->videoStream()->getCodecParameters();
    if (!codecPar)
        return AVERROR_DECODER_NOT_FOUND; //< Using as stream not found.
    if (nx::utils::AppInfo::isRaspberryPi() && codecPar->codec_id == AV_CODEC_ID_H264)
        status = decoder->initializeDecoder("h264_mmal");
    else
        status = decoder->initializeDecoder(codecPar);

    if (status < 0)
        return status;

    status = decoder->open();
    if (status < 0)
        return status;

    m_decoder = std::move(decoder);
    return 0;
}

std::shared_ptr<ffmpeg::Frame> TranscodeStreamReader::decode(const ffmpeg::Packet * packet)
{
    if (!m_decoder)
        return nullptr;

    m_decoderTimestamps.addTimestamp(packet->pts(), packet->timestamp());
    int status = m_decoder->sendPacket(packet->packet());

    auto frame = std::make_shared<ffmpeg::Frame>();
    if (status == 0 || status == AVERROR(EAGAIN))
        status = m_decoder->receiveFrame(frame->frame());

    if (status < 0)
        return nullptr;

    if (frame->pts() == AV_NOPTS_VALUE)
        frame->frame()->pts = frame->packetPts();

    auto nxTimestamp = m_decoderTimestamps.takeNxTimestamp(frame->packetPts());
    if (!nxTimestamp.has_value())
    {
        NX_DEBUG(this, "Unknown pts %1", frame->packetPts());
        nxTimestamp.emplace(m_camera->millisSinceEpoch());
    }

    frame->setTimestamp(nxTimestamp.value());
    return frame;
}

} // namespace usb_cam
} // namespace nx
