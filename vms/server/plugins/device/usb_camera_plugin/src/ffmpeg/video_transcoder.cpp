#include "video_transcoder.h"

#include "ffmpeg/utils.h"
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>

namespace nx::usb_cam::ffmpeg {

const std::vector<AVCodecID> kEncoderCodecPriorityList =
    { AV_CODEC_ID_H263P, AV_CODEC_ID_MJPEG };

int VideoTranscoder::initializeDecoder(AVCodecParameters* codecPar)
{
    auto decoder = std::make_unique<ffmpeg::Codec>();
    int status;
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

int VideoTranscoder::initializeEncoder(const CodecParameters& codecParams)
{
    int result = 0;
    std::unique_ptr<ffmpeg::Codec> encoder;
    for (const auto & codecId: kEncoderCodecPriorityList)
    {
        encoder = std::make_unique<ffmpeg::Codec>();
        result = encoder->initializeEncoder(codecId);
        if (result >= 0)
            break;
    }
    if (result < 0)
        return result;

    encoder->setFps(codecParams.fps);
    encoder->setResolution(codecParams.resolution.width, codecParams.resolution.height);
    encoder->setBitrate(codecParams.bitrate);
    encoder->setPixelFormat(ffmpeg::utils::suggestPixelFormat(encoder->codec()));

    AVCodecContext* context = encoder->codecContext();
    context->mb_decision = FF_MB_DECISION_BITS;
    // Don't use global header. the rpi cam doesn't stream properly with global.
    context->flags = AV_CODEC_FLAG2_LOCAL_HEADER;
    context->global_quality = context->qmin * FF_QP2LAMBDA;
    context->gop_size = codecParams.fps;

    result = encoder->open();
    if (result < 0)
        return result;

    m_encoder = std::move(encoder);
    return 0;
}

int VideoTranscoder::initialize(
    AVCodecParameters* decoderCodecPar,const CodecParameters& codecParams)
{
    int status = initializeDecoder(decoderCodecPar);
    if (status < 0)
        return status;

    status = initializeEncoder(codecParams);
    if (status < 0)
        return status;

    m_scaler.configure(codecParams.resolution, m_encoder->codec()->pix_fmts[0]);
    m_codecParams = codecParams;
    return status;
}

std::shared_ptr<ffmpeg::Frame> VideoTranscoder::decode(const ffmpeg::Packet * packet)
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
    if (!nxTimestamp.has_value() && m_timeGetter)
    {
        NX_DEBUG(this, "Unknown pts %1", frame->packetPts());
        nxTimestamp.emplace(m_timeGetter());
    }

    frame->setTimestamp(nxTimestamp.value());
    return frame;
}

int VideoTranscoder::encode(const ffmpeg::Frame* frame, ffmpeg::Packet * outPacket)
{
    int result = m_encoder->sendFrame(frame->frame());
    if (result == 0 || result == AVERROR(EAGAIN))
        return m_encoder->receivePacket(outPacket->packet());
    return result;
}

bool VideoTranscoder::shouldDrop(const ffmpeg::Frame* frame)
{
    if (!frame)
        return true;

    int64_t timePerFrameMSec = 1.0 / m_codecParams.fps * 1000;
    bool drop = int64_t(frame->timestamp()) - m_lastTimestamp < timePerFrameMSec;
    if (!drop)
        m_lastTimestamp = frame->timestamp();

    return drop;
}

int VideoTranscoder::transcode(
    const ffmpeg::Packet* source, std::shared_ptr<ffmpeg::Packet>& result)
{
    auto frame = decode(source);
    if (shouldDrop(frame.get()))
        return AVERROR(EAGAIN);

    Frame* scaledFrame;
    int status = m_scaler.scale(frame.get(), &scaledFrame);
    if (status < 0)
        return status;

    result = std::make_shared<ffmpeg::Packet>(m_encoder->codecId(), AVMEDIA_TYPE_VIDEO);

    status = encode(scaledFrame, result.get());
    if (status < 0)
        return status;

    m_encoderTimestamps.addTimestamp(frame->packetPts(), frame->timestamp());

    auto nxTimestamp = m_encoderTimestamps.takeNxTimestamp(result->pts());
    if (!nxTimestamp.has_value() && m_timeGetter)
        nxTimestamp.emplace(m_timeGetter());

    result->setTimestamp(nxTimestamp.value());
    return status;
}

void VideoTranscoder::uninitialize()
{
    m_scaler.uninitialize();
    if (m_decoder)
        m_decoder->flush();
    m_decoder.reset(nullptr);
    m_encoder.reset(nullptr);
}

} // namespace nx::usb_cam::ffmpeg
