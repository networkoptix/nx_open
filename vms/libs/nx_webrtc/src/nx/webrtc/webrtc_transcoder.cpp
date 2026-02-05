// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_transcoder.h"

#include <common/common_module.h>
#include <nx/codec/h265/hevc_decoder_configuration_record.h>
#include <nx/codec/mime.h>
#include <nx/codec/nal_extradata.h>
#include <transcoding/transcoding_utils.h>

#include "nx_webrtc_ini.h"
#include "webrtc_session.h"
#include "webrtc_session_pool.h"

namespace {

static const std::set<AVCodecID> kSrtpVideoCodecs =
{
    AV_CODEC_ID_VP8,
    AV_CODEC_ID_VP9,
    AV_CODEC_ID_H264,
    AV_CODEC_ID_H265
};

static const std::set<AVCodecID> kSrtpAudioCodecs =
{
    AV_CODEC_ID_PCM_MULAW,
    AV_CODEC_ID_PCM_ALAW,
    AV_CODEC_ID_ADPCM_G722
};

static const std::set<AVCodecID> kMseVideoCodecs =
{
    AV_CODEC_ID_H264,
    AV_CODEC_ID_H265
};

static const std::set<AVCodecID> kMseAudioCodecs =
{
    // PCM* codecs are also supported by browser side, but there is no ffmpeg mp4 muxing for them.
    AV_CODEC_ID_AAC,
    AV_CODEC_ID_MP2,
    AV_CODEC_ID_MP3,
    AV_CODEC_ID_AC3,
    AV_CODEC_ID_EAC3,
};

/* For some NATs, there can be another network layer, such as VPN,
 * which fragments original UDP packets with RTP.
 * Note that RTP packets with UDP transport can't be reassembled:
 * there is no information about original RTP packet size in RTP packet via UDP.
 * */
static const int kWebrtcMtu = 1300;

}

namespace nx::webrtc {

Transcoder::Transcoder(
    Session* session,
    nx::vms::api::WebRtcMethod method,
    nx::vms::api::WebRtcTrackerSettings::MseFormat mseFormat):
    m_session(session), m_method(method), m_mseFormat(mseFormat)
{
}

bool Transcoder::createEncoders(
    AVCodecParameters* videoParameters, AVCodecParameters* audioParameters)
{
    if (m_method == nx::vms::api::WebRtcMethod::srtp)
        return createRtpEncoders(videoParameters, audioParameters);
    else
        return createMseEncoder(videoParameters, audioParameters);
}

bool Transcoder::createRtpEncoders(
    AVCodecParameters* videoParameters, AVCodecParameters* audioParameters)
{
    if (videoParameters)
    {
        const auto& track = m_session->tracks()->videoTrack();
        m_videoEncoder = createRtpEncoder(
            videoParameters, track.ssrc, track.cname);
    }

    if (audioParameters)
    {
        const auto& track = m_session->tracks()->audioTrack();
        m_audioEncoder = createRtpEncoder(
            audioParameters, track.ssrc, track.cname);
    }

    if (!m_videoEncoder && !m_audioEncoder)
        return false;

    return true;
}

bool Transcoder::isVideoCodecSupported(AVCodecID codecId)
{
    if (m_method == nx::vms::api::WebRtcMethod::srtp)
    {
        if (kSrtpVideoCodecs.count(codecId) == 0)
            return false;
        if (codecId == AV_CODEC_ID_H265 && !::ini().webrtcSrtpTransportEnableH265)
            return false;
        return true;
    }
    else
    {
        return kMseVideoCodecs.count(codecId) != 0;
    }
}

bool Transcoder::isAudioCodecSupported(AVCodecID codecId)
{
    if (m_method == nx::vms::api::WebRtcMethod::srtp)
    {
        return kSrtpAudioCodecs.count(codecId) != 0;
    }
    else
    {
        return kMseAudioCodecs.count(codecId) != 0;
    }
}

bool Transcoder::createMseEncoder(
    AVCodecParameters* videoParameters, AVCodecParameters* audioParameters)
{
    if (!videoParameters && !audioParameters)
    {
        NX_DEBUG(this, "Can't create MSE transcoder without video and audio");
        return false;
    }

    m_mseMuxer = std::make_unique<FfmpegMuxer>(FfmpegMuxer::Config());
    m_mseMuxer->setContainer(nx::toString(m_mseFormat));
    if (m_mseFormat == nx::vms::api::WebRtcTrackerSettings::MseFormat::mp4)
    {
        m_mseMuxer->setFormatOption(
          "movflags", "empty_moov+omit_tfhd_offset+frag_keyframe+default_base_moof+isml");
    }

    if (videoParameters && !m_mseMuxer->addVideo(videoParameters))
    {
        NX_DEBUG(this, "Can't add video stream to MSE muxer");
        return false;
    }

    if (audioParameters && !m_mseMuxer->addAudio(audioParameters))
    {
        NX_DEBUG(this, "Can't add audio stream to MSE muxer");
        return false;
    }

    if (!m_mseMuxer->open())
    {
        NX_DEBUG(this, "Can't open MSE muxer");
        return false;
    }

    return constructMimeType();
}

QnUniversalRtpEncoderPtr Transcoder::createRtpEncoder(
    AVCodecParameters* codecParameters,
    uint32_t ssrc,
    const std::string& cname)
{
    QnUniversalRtpEncoder::Config config;
    config.absoluteRtcpTimestamps = true;
    config.addOnvifHeaderExtension = false;
    config.useMultipleSdpPayloadTypes = false;
    config.useRtcpNack = true;
    config.webRtcMode = ::ini().webrtcFixH264ProfileConstraints;

    QnUniversalRtpEncoderPtr universalEncoder(new QnUniversalRtpEncoder(config, nullptr));
    universalEncoder->setSsrc(ssrc);
    universalEncoder->setCName(cname);
    universalEncoder->setMtu(kWebrtcMtu);

    if (!universalEncoder->open(codecParameters))
    {
        NX_DEBUG(this, "%1: No RTSP encoder for codec %2 skip track",
            m_session->id(), codecParameters->codec_id);
        return nullptr;
    }

    if (codecParameters->codec_type == AVMEDIA_TYPE_VIDEO)
        m_session->tracks()->setVideoPayloadType(universalEncoder->payloadType());
    else if (codecParameters->codec_type == AVMEDIA_TYPE_AUDIO)
        m_session->tracks()->setAudioPayloadType(universalEncoder->payloadType());

    return universalEncoder;
}

void Transcoder::setSrtpEncryptionData(const rtsp::EncryptionData& data)
{
    if (m_videoEncoder)
        m_videoEncoder->setSrtpEncryptionData(data);
    if (m_audioEncoder)
        m_audioEncoder->setSrtpEncryptionData(data);
}

rtsp::SrtpEncryptor* Transcoder::getEncryptor() const
{
    if (m_videoEncoder)
        return m_videoEncoder->encryptor();
    else if (m_audioEncoder)
        return m_audioEncoder->encryptor();
    return nullptr;
}

QnUniversalRtpEncoder* Transcoder::videoEncoder() const
{
    return m_videoEncoder.get();
}

QnUniversalRtpEncoder* Transcoder::audioEncoder() const
{
    return m_audioEncoder.get();
}

bool Transcoder::constructMimeType()
{

    m_mimeType = nx::media::getFormatMimeType(m_mseMuxer->container().toStdString());
    if (m_mimeType.empty())
    {
        NX_DEBUG(this, "Failed to build mime type, container not supported: %1",
            m_mseMuxer->container());
        return false;
    }

    const auto* videoCodecParameters = m_mseMuxer->getVideoCodecParameters();
    const auto* audioCodecParameters = m_mseMuxer->getAudioCodecParameters();

    auto videoStr = nx::media::getMimeType(videoCodecParameters);
    auto audioStr = nx::media::getMimeType(audioCodecParameters);
    if (!videoStr.empty() || !audioStr.empty())
        m_mimeType += "; codecs=\"";
    if (!videoStr.empty())
    {
        m_mimeType += videoStr;
        if (!audioStr.empty())
            m_mimeType += ",";
    }
    if (!audioStr.empty())
    {
        m_mimeType += audioStr;
    }
    if (!videoStr.empty() || !audioStr.empty())
        m_mimeType += "\"";

    return true;
}

QnUniversalRtpEncoder* Transcoder::encoderBySsrc(uint32_t ssrc) const
{
    if (m_session->tracks()->videoTrack().ssrc == ssrc)
        return videoEncoder();
    else if (m_session->tracks()->audioTrack().ssrc == ssrc)
        return audioEncoder();
    return nullptr;
}

QnUniversalRtpEncoder* Transcoder::getRtpEncoder(QnAbstractMediaData::DataType type) const
{
    if (type == QnAbstractMediaData::VIDEO)
        return m_videoEncoder.get();
    else if (type == QnAbstractMediaData::AUDIO)
        return m_audioEncoder.get();
    return nullptr;
}

bool Transcoder::checkForMseEof(QnConstAbstractMediaDataPtr media)
{
    NX_ASSERT(m_method == nx::vms::api::WebRtcMethod::mse, "Should be called only with MSE");

    // Checks for EOF.
    // 1. If EOF is already set, you can only re-create Transcoder.
    if (m_eof)
        return true;

    // 2. Check for codec changed on-fly.
    auto videoParameters = m_mseMuxer->getVideoCodecParameters();
    auto audioParameters = m_mseMuxer->getAudioCodecParameters();

    if ((media->dataType == QnAbstractMediaData::VIDEO && !videoParameters)
        || (media->dataType == QnAbstractMediaData::AUDIO && !audioParameters))
    {
        NX_DEBUG(this, "MSE transcoder got EOF, appeared track that is not announced: %1", media);
        return true; //< New appeared track.
    }

    if (media->dataType == QnAbstractMediaData::VIDEO
        && videoParameters
        && media->compressionType != videoParameters->codec_id)
    {
        NX_DEBUG(this, "MSE transcoder got EOF for video: has %1, got %2",
            media->compressionType, videoParameters ? videoParameters->codec_id : AV_CODEC_ID_NONE);
        return true;
    }
    else if (media->dataType == QnAbstractMediaData::AUDIO
        && audioParameters
        && media->compressionType != audioParameters->codec_id)
    {
        NX_DEBUG(this, "MSE transcoder got EOF for audio: has %1, got %2",
            media->compressionType, audioParameters ? audioParameters->codec_id : AV_CODEC_ID_NONE);
        return true;
    }

    // 3. Check for audio or video track disappear.
    if (videoParameters && audioParameters)
    {
        if (media->dataType == QnAbstractMediaData::VIDEO)
        {
            constexpr int kMinVideoInGop = 7; //< Special const for strange small GOPs.
            if (media->flags & AV_PKT_FLAG_KEY)
            {
                if (m_audioPacketsCount == 0 && m_videoPacketsCount > kMinVideoInGop)
                {
                    NX_DEBUG(this, "MSE transcoder got EOF: not enough audio for gop: %1",
                        m_videoPacketsCount);
                    return true;
                }
                m_videoPacketsCount = m_audioPacketsCount = 0;
            }
            ++m_videoPacketsCount;
        }
        else if (media->dataType == QnAbstractMediaData::AUDIO)
        {
            ++m_audioPacketsCount;
        }
    }

    return false;
}

void Transcoder::setDataPacket(QnConstAbstractMediaDataPtr media)
{
    m_lastTimestampMs = media->timestamp / 1000;
    if (m_method == nx::vms::api::WebRtcMethod::srtp)
    {
        QnUniversalRtpEncoder* rtpEncoder = getRtpEncoder(media->dataType);
        if (!rtpEncoder)
            return; //< Skip not supported data.
        rtpEncoder->setDataPacket(media);
    }
    else
    {
        NX_ASSERT(m_mseMuxer);
        // Checks for inappropriate data.
        if (media->dataType != QnAbstractMediaData::VIDEO
            && media->dataType != QnAbstractMediaData::AUDIO)
        {
            return; //< Skip not supported data.
        }
        m_eof = checkForMseEof(media);
        if (m_eof)
            return;

        if (!m_mseMuxer->process(media))
        {
            NX_DEBUG(this, "MSE muxer: failed to process data");
            m_eof = true;
            return;
        }

        nx::utils::ByteArray buffer;
        m_mseMuxer->getResult(&buffer);
        if (buffer.size() != 0)
            m_mseBuffers.emplace(std::move(buffer));
    }
}

bool Transcoder::getNextPacket(QnAbstractMediaData::DataType type, nx::utils::ByteArray& buffer)
{
    if (m_method == nx::vms::api::WebRtcMethod::srtp)
    {
        QnUniversalRtpEncoder* rtpEncoder = getRtpEncoder(type);
        if (!rtpEncoder)
            return false; //< Skip not supported data.
        return rtpEncoder->getNextPacket(buffer);
    }
    else
    {
        if (!m_mseBuffers.empty())
        {
            buffer = std::move(m_mseBuffers.front());
            m_mseBuffers.pop();
            return true;
        }
        return false;
    }
}

bool Transcoder::isEof(QnAbstractMediaData::DataType type) const
{
    if (m_eof)
        return true;

    if (m_method == nx::vms::api::WebRtcMethod::srtp)
    {
        QnUniversalRtpEncoder* rtpEncoder = getRtpEncoder(type);
        if (!rtpEncoder)
            return false; //< Skip not supported data.
        return rtpEncoder->isEof();
    }
    else
    {
        return !m_mseMuxer;
    }
}

FfmpegMuxer::PacketTimestamp Transcoder::getLastTimestamps() const
{
    if (m_method == nx::vms::api::WebRtcMethod::srtp)
    {
        QnUniversalRtpEncoder* rtpEncoder = nullptr;
        if (m_videoEncoder)
            rtpEncoder = m_videoEncoder.get();
        else if (m_audioEncoder)
            rtpEncoder = m_audioEncoder.get();

        if (!rtpEncoder)
            return FfmpegMuxer::PacketTimestamp(); //< Skip not supported data.
        return rtpEncoder->getLastTimestamps();
    }
    else
    {
        NX_ASSERT(m_mseMuxer);
        return m_mseMuxer->getLastPacketTimestamp();
    }
}

} // namespace nx::webrtc
