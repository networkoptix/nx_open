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
    nx::Uuid deviceId,
    AVCodecParameters* videoParameters,
    AVCodecParameters* audioParameters)
{
    if (m_method == nx::vms::api::WebRtcMethod::srtp)
        return createRtpEncoders(deviceId, videoParameters, audioParameters);
    else
        return createMseEncoder(videoParameters, audioParameters);
}

bool Transcoder::createRtpEncoders(
    nx::Uuid deviceId,
    AVCodecParameters* videoParameters,
    AVCodecParameters* audioParameters)
{
    bool success = false;
    if (videoParameters)
    {
        if (auto track = m_session->tracks()->videoTrack(deviceId))
        {
            if (auto encoder = createRtpEncoder(videoParameters, track->ssrc, track->cname, track->mid))
            {
                success = true;
                if (m_encryptionData.has_value())
                    encoder->setSrtpEncryptionData(*m_encryptionData);
                track->offerState = TrackState::active;
                track->payloadType = encoder->payloadType();
                m_videoEncoders[deviceId] = std::move(encoder);
            }
            else
            {
                NX_DEBUG(this, "Can't create Video encoder for track %1, codec: %2, device: %3",
                    track->ssrc, videoParameters->codec_id, track->deviceId);
            }
        }
    }

    if (audioParameters)
    {
        if (auto track = m_session->tracks()->audioTrack(deviceId))
        {
            if (auto encoder = createRtpEncoder(audioParameters, track->ssrc, track->cname, track->mid))
            {
                success = true;
                if (m_encryptionData.has_value())
                    encoder->setSrtpEncryptionData(*m_encryptionData);
                track->offerState = TrackState::active;
                track->payloadType = encoder->payloadType();
                m_audioEncoders[deviceId] = std::move(encoder);
            }
            else
            {
                NX_DEBUG(this, "Can't create Audio encoder for track %1, codec: %2, device: %3",
                    track->ssrc, audioParameters->codec_id, track->deviceId);
            }
        }
    }

    return success;
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
    const std::string& cname,
    int /*mid*/)
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

    return universalEncoder;
}

void Transcoder::setSrtpEncryptionData(const rtsp::EncryptionData& data)
{
    m_encryptionData = data;
    for (auto& [_, encoder]: m_videoEncoders)
        encoder->setSrtpEncryptionData(data);
    for (auto& [_, encoder]:  m_audioEncoders)
        encoder->setSrtpEncryptionData(data);
}

rtsp::SrtpEncryptor* Transcoder::getEncryptor() const
{
    if (!m_videoEncoders.empty())
        return m_videoEncoders.begin()->second->encryptor();
    if (!m_audioEncoders.empty())
        return m_audioEncoders.begin()->second->encryptor();
    return nullptr;
}

QnUniversalRtpEncoder* Transcoder::videoEncoder(nx::Uuid deviceId) const
{
    auto it = m_videoEncoders.find(deviceId);
    return it != m_videoEncoders.end() ? it->second.get() : nullptr;
}

QnUniversalRtpEncoder* Transcoder::audioEncoder(nx::Uuid deviceId) const
{
    auto it = m_audioEncoders.find(deviceId);
    return it != m_audioEncoders.end() ? it->second.get() : nullptr;
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
    if (auto track = m_session->tracks()->track(ssrc))
    {
        if (track->trackType == TrackType::video)
            return videoEncoder(track->deviceId);
        else if (track->trackType == TrackType::audio)
            return audioEncoder(track->deviceId);
    }
    return nullptr;
}

QnUniversalRtpEncoder* Transcoder::getRtpEncoder(
    nx::Uuid deviceId,
    QnAbstractMediaData::DataType type) const
{
    auto& encoders = type == QnAbstractMediaData::VIDEO ? m_videoEncoders : m_audioEncoders;
    auto it = encoders.find(deviceId);
    return it != encoders.end() ? it->second.get() : nullptr;
}

bool Transcoder::checkForMseEof(QnConstAbstractMediaDataPtr media)
{
    NX_ASSERT(m_method == nx::vms::api::WebRtcMethod::mse, "Should be called only with MSE");

    // Checks for EOF.
    // 1. If EOF is already set, you can only re-create Transcoder.
    if (!m_eof.empty())
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

QnUniversalRtpEncoder* Transcoder::setDataPacket(nx::Uuid deviceId, QnConstAbstractMediaDataPtr media)
{
    m_lastTimestampMs = media->timestamp / 1000;
    if (m_method == nx::vms::api::WebRtcMethod::srtp)
    {
        QnUniversalRtpEncoder* rtpEncoder = getRtpEncoder(deviceId, media->dataType);
        if (!rtpEncoder)
            return nullptr; //< Skip not supported data.

        rtpEncoder->setDataPacket(media);
        return rtpEncoder;
    }
    else
    {
        NX_ASSERT(m_mseMuxer);
        // Checks for inappropriate data.
        if (media->dataType != QnAbstractMediaData::VIDEO
            && media->dataType != QnAbstractMediaData::AUDIO)
        {
            return nullptr; //< Skip not supported data.
        }
        if (checkForMseEof(media))
        {
            m_eof.insert(deviceId);
            return nullptr;
        }

        if (!m_mseMuxer->process(media))
        {
            NX_DEBUG(this, "MSE muxer: failed to process data");
            m_eof.insert(deviceId);
            return nullptr;
        }

        nx::utils::ByteArray buffer;
        m_mseMuxer->getResult(&buffer);
        if (buffer.size() != 0)
            m_mseBuffers.emplace(std::move(buffer));
        return nullptr;
    }
}

bool Transcoder::getNextPacket(
    QnUniversalRtpEncoder* rtpEncoder,
    nx::utils::ByteArray& buffer)
{
    if (m_method == nx::vms::api::WebRtcMethod::srtp)
    {
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

bool Transcoder::isEof(
    nx::Uuid deviceId,
    QnUniversalRtpEncoder* rtpEncoder) const
{
    if (m_eof.contains(deviceId))
        return true;

    if (m_method == nx::vms::api::WebRtcMethod::srtp)
    {
        return rtpEncoder->isEof();
    }
    else
    {
        return !m_mseMuxer;
    }
}

FfmpegMuxer::PacketTimestamp Transcoder::getLastTimestamps() const
{
    // TODO: this function doesn't support multi camera streaming properly. It is needed to refactor it.
    if (m_method == nx::vms::api::WebRtcMethod::srtp)
    {
        QnUniversalRtpEncoder* rtpEncoder = nullptr;
        if (!m_videoEncoders.empty())
            rtpEncoder = m_videoEncoders.begin()->second.get();
        else if (!m_audioEncoders.empty())
            rtpEncoder = m_videoEncoders.begin()->second.get();

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
