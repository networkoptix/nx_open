// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_transcoder.h"

#include <nx/codec/h265/hevc_decoder_configuration_record.h>
#include <nx/codec/nal_extradata.h>
#include <transcoding/transcoding_utils.h>

#include "nx_webrtc_ini.h"
#include "webrtc_session.h"
#include "webrtc_session_pool.h"

namespace {

// The payload type of each codec that can appear on an m-line of a sending session. See
// nx::webrtc::rtpPayloadType(). 96 is what H.264 has been offered under all along; 97 is the
// dynamic audio payload type of QnUniversalRtpEncoder and 98 its secondary stream, so H.265
// starts above them, and VP8 and VP9 above it: without a number of their own they both fall back
// to the dynamic 96, under which the client reads them as H.264. The audio codecs have payload
// types assigned to them statically by RFC 3551 table 4, which is what getPayloadType() gives
// them as well.
static const std::map<AVCodecID, int> kVideoPayloadTypes = {
    {AV_CODEC_ID_H264, 96},
    {AV_CODEC_ID_H265, 100},
    {AV_CODEC_ID_VP8, 101},
    {AV_CODEC_ID_VP9, 102},
};

static const std::map<AVCodecID, int> kAudioPayloadTypes = {
    {AV_CODEC_ID_PCM_MULAW, 0},
    {AV_CODEC_ID_PCM_ALAW, 8},
    {AV_CODEC_ID_ADPCM_G722, 9},
};

const std::map<AVCodecID, int>& payloadTypesOf(AVMediaType mediaType)
{
    static const std::map<AVCodecID, int> kNone;
    switch (mediaType)
    {
        case AVMEDIA_TYPE_VIDEO:
            return kVideoPayloadTypes;
        case AVMEDIA_TYPE_AUDIO:
            return kAudioPayloadTypes;
        default:
            return kNone;
    }
}

static const std::set<AVCodecID> kSrtpVideoCodecs = {
    AV_CODEC_ID_VP8, AV_CODEC_ID_VP9, AV_CODEC_ID_AV1, AV_CODEC_ID_H264, AV_CODEC_ID_H265};

static const std::set<AVCodecID> kSrtpAudioCodecs = {
    AV_CODEC_ID_PCM_MULAW, AV_CODEC_ID_PCM_ALAW, AV_CODEC_ID_ADPCM_G722, AV_CODEC_ID_OPUS};

/* For some NATs, there can be another network layer, such as VPN,
 * which fragments original UDP packets with RTP.
 * Note that RTP packets with UDP transport can't be reassembled:
 * there is no information about original RTP packet size in RTP packet via UDP.
 * */
static const int kWebrtcMtu = 1300;

// Everything the m-line of `encoder` offers: the codec it streams and the ones it may switch to.
std::set<int> offeredPayloadTypes(const QnUniversalRtpEncoder& encoder)
{
    std::set<int> result = {encoder.payloadType()};
    for (const int payloadType: encoder.alternativePayloadTypes())
        result.insert(payloadType);
    return result;
}

}

namespace nx::webrtc {

std::optional<int> rtpPayloadType(AVCodecID codecId)
{
    for (const auto* table: {&kVideoPayloadTypes, &kAudioPayloadTypes})
    {
        if (const auto it = table->find(codecId); it != table->end())
            return it->second;
    }
    return std::nullopt;
}

QStringList alternativeCodecSdp(AVCodecID codecId, int payloadType)
{
    const QString pt = QString::number(payloadType);
    switch (codecId)
    {
        case AV_CODEC_ID_H264:
        {
            // No sprop-parameter-sets: what the parameter sets of this codec will be is not
            // known until a stream of it starts, and H2645PrependParameterSets puts them in
            // front of every key frame anyway. level-asymmetry-allowed says the stream may run
            // at a level of its own (RFC 6184 section 8.2.2), while the profile may not be
            // changed by the answer, hence the ini parameter.
            QString fmtp = "a=fmtp:" + pt + " packetization-mode=1;level-asymmetry-allowed=1";
            const QString profileLevelId = ::ini().webrtcAlternativeH264ProfileLevelId;
            if (!profileLevelId.isEmpty())
                fmtp += ";profile-level-id=" + profileLevelId;
            return {"a=rtpmap:" + pt + " H264/90000", fmtp};
        }
        case AV_CODEC_ID_VP8:
            return {"a=rtpmap:" + pt + " VP8/90000"};
        case AV_CODEC_ID_VP9:
            return {"a=rtpmap:" + pt + " VP9/90000"};
        case AV_CODEC_ID_H265:
            // Left without an fmtp line on purpose: with no profile-id, tier-flag or level-id
            // the defaults of RFC 7798 section 7.1 apply (Main profile, main tier, level 3.1),
            // which is exactly what a streaming m-line says today -- ffmpeg writes none of them
            // for H.265 either, only the sprop-* parameter sets.
            return {"a=rtpmap:" + pt + " H265/90000"};
        // The audio codecs take no parameters, and all three run at the same RTP clock rate:
        // 8000 even for G.722, which RFC 3551 section 4.5.2 keeps at the value it was given by
        // mistake. That is what lets them share an m-line and an ssrc the way H.264 and H.265 do.
        case AV_CODEC_ID_PCM_MULAW:
            return {"a=rtpmap:" + pt + " PCMU/8000"};
        case AV_CODEC_ID_PCM_ALAW:
            return {"a=rtpmap:" + pt + " PCMA/8000"};
        case AV_CODEC_ID_ADPCM_G722:
            return {"a=rtpmap:" + pt + " G722/8000"};
        default:
            return {};
    }
}

Transcoder::Transcoder(Session* session): m_session(session)
{
}

bool Transcoder::createEncoders(
    nx::Uuid deviceId, AVCodecParameters* videoParameters, AVCodecParameters* audioParameters)
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
                track->offeredPayloadTypes = offeredPayloadTypes(*encoder);
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
                track->offeredPayloadTypes = offeredPayloadTypes(*encoder);
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
    if (kSrtpVideoCodecs.count(codecId) == 0)
        return false;
    if (codecId == AV_CODEC_ID_H265 && !::ini().webrtcSrtpTransportEnableH265)
        return false;
    return true;
}

bool Transcoder::isAudioCodecSupported(AVCodecID codecId)
{
    return kSrtpAudioCodecs.count(codecId) != 0;
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
    // Without a payload type of its own a codec cannot share an m-line with another one, which
    // is what switching to it without a new offer takes.
    config.payloadType = rtpPayloadType(codecParameters->codec_id);

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

    // Every other codec this Device could switch to is offered on the m-line as well, so that
    // the switch itself takes no new SDP: it changes nothing but the payload type of the
    // packets, which the client has already accepted. See Session::switchCodec().
    if (config.payloadType)
    {
        const bool isVideo = codecParameters->codec_type == AVMEDIA_TYPE_VIDEO;
        std::vector<int> payloadTypes;
        QStringList sdpAttributes;
        for (const auto& [codecId, payloadType]: payloadTypesOf(codecParameters->codec_type))
        {
            const bool supported =
                isVideo ? isVideoCodecSupported(codecId) : isAudioCodecSupported(codecId);
            if (codecId == codecParameters->codec_id || !supported)
                continue;

            payloadTypes.push_back(payloadType);
            sdpAttributes.append(alternativeCodecSdp(codecId, payloadType));
        }
        universalEncoder->setAlternativeCodecs(std::move(payloadTypes), std::move(sdpAttributes));
    }

    return universalEncoder;
}

void Transcoder::removeEncoders(nx::Uuid deviceId)
{
    m_videoEncoders.erase(deviceId);
    m_audioEncoders.erase(deviceId);
}

void Transcoder::removeEncoder(nx::Uuid deviceId, QnAbstractMediaData::DataType videoOrAudio)
{
    if (videoOrAudio == QnAbstractMediaData::VIDEO)
        m_videoEncoders.erase(deviceId);
    else if (videoOrAudio == QnAbstractMediaData::AUDIO)
        m_audioEncoders.erase(deviceId);
}

QnUniversalRtpEncoder::Continuity Transcoder::takeContinuity(
    nx::Uuid deviceId, QnAbstractMediaData::DataType videoOrAudio)
{
    auto* encoder = getRtpEncoder(deviceId, videoOrAudio);
    if (!NX_ASSERT(
            encoder, "No %1 encoder of device %2 to take over from", videoOrAudio, deviceId))
    {
        return {};
    }

    return encoder->takeContinuity();
}

void Transcoder::restoreContinuity(nx::Uuid deviceId,
    QnAbstractMediaData::DataType videoOrAudio,
    QnUniversalRtpEncoder::Continuity continuity)
{
    auto* encoder = getRtpEncoder(deviceId, videoOrAudio);
    if (!NX_ASSERT(encoder, "No %1 encoder of device %2 to continue with", videoOrAudio, deviceId))
        return;

    encoder->restoreContinuity(std::move(continuity));
}

void Transcoder::setSrtpEncryptionData(const rtsp::EncryptionData& data)
{
    m_encryptionData = data;
    m_decryptor = std::make_unique<rtsp::SrtpDecryptor>();
    if (!m_decryptor->init(data.client))
        m_decryptor.reset();
    for (auto& [_, encoder]: m_videoEncoders)
        encoder->setSrtpEncryptionData(data);
    for (auto& [_, encoder]:  m_audioEncoders)
        encoder->setSrtpEncryptionData(data);
}

rtsp::SrtpDecryptor* Transcoder::getDecryptor() const
{
    return m_decryptor.get();
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
    if (type == QnAbstractMediaData::VIDEO)
        return videoEncoder(deviceId);
    if (type == QnAbstractMediaData::AUDIO)
        return audioEncoder(deviceId);

    return nullptr;
}

QnUniversalRtpEncoder* Transcoder::setDataPacket(nx::Uuid deviceId, QnConstAbstractMediaDataPtr media)
{
    m_lastTimestampMs = media->timestamp / 1000;

    // Whatever is packetized for a track the client did not take, or for one that is gone along
    // with its Device, is discarded on arrival at best, so spend neither the CPU nor the
    // bandwidth on it. See Session::processSdpAnswer() and Session::dropProvider().
    const Track* track = nullptr;
    if (media->dataType == QnAbstractMediaData::VIDEO)
        track = m_session->tracks()->videoTrack(deviceId);
    else if (media->dataType == QnAbstractMediaData::AUDIO)
        track = m_session->tracks()->audioTrack(deviceId);

    if (!track || track->offerState != TrackState::active)
        return nullptr;

    QnUniversalRtpEncoder* rtpEncoder = getRtpEncoder(deviceId, media->dataType);
    if (!rtpEncoder)
        return nullptr; //< Skip not supported data.

    rtpEncoder->setDataPacket(media);
    return rtpEncoder;
}

FfmpegMuxer::PacketTimestamp Transcoder::getLastTimestamps() const
{
    // TODO: this function doesn't support multi camera streaming properly. It is needed to refactor it.
    QnUniversalRtpEncoder* rtpEncoder = nullptr;
    if (!m_videoEncoders.empty())
        rtpEncoder = m_videoEncoders.begin()->second.get();
    else if (!m_audioEncoders.empty())
        rtpEncoder = m_audioEncoders.begin()->second.get();

    if (!rtpEncoder)
        return FfmpegMuxer::PacketTimestamp(); //< Skip not supported data.
    return rtpEncoder->getLastTimestamps();
}

} // namespace nx::webrtc
