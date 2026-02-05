// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_tracks.h"

#include <core/resource/camera_resource.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/utils/random.h>

#include "webrtc_session.h"

namespace nx::webrtc {

namespace {

void fillTrack(Track& track, const std::string& mid, Purpose purpose)
{
    using namespace nx::sdk::UuidHelper;
    track.ssrc = nx::utils::random::number<uint32_t>();
    track.cname = toStdString(randomUuid(), FormatOptions::hyphens);
    track.msid = toStdString(randomUuid(), FormatOptions::hyphens);
    track.mid = mid;
    track.purpose = purpose;
}

static const std::string kSendOnlyAttr = "a=sendonly";
static const std::string kRecvOnlyAttr = "a=recvonly";
static const std::string kSendRecvAttr = "a=sendrecv";

} // namespace

const std::string& toSdpAttribute(Purpose purpose)
{
    switch (purpose)
    {
    case Purpose::send:
        return kSendOnlyAttr;
    case Purpose::recv:
        return kRecvOnlyAttr;
    case Purpose::sendrecv:
    default:
        return kSendRecvAttr;
    }
}

Tracks::Tracks(Session* session): m_session(session)
{
    fillTrack(m_localVideoTrack, "0", session->purpose());
    fillTrack(m_localAudioTrack, "1", session->purpose());
}

void Tracks::setVideoPayloadType(int payloadType)
{
    m_localVideoTrack.payloadType = payloadType;
}

void Tracks::setAudioPayloadType(int payloadType)
{
    m_localAudioTrack.payloadType = payloadType;
}

TracksForSend::TracksForSend(Session* session): Tracks(session)
{
}

bool TracksForSend::hasVideo() const
{
    return m_session->muxer()->videoEncoder() != nullptr;
}

bool TracksForSend::hasAudio() const
{
    return m_session->muxer()->audioEncoder() != nullptr;
}

std::string TracksForSend::getVideoMedia(uint16_t port) const
{
    if (!NX_ASSERT(hasVideo()))
        return "";
    return m_session->muxer()->videoEncoder()->getSdpMedia(
        true, 0, port, /*ssl*/ true).toStdString();
}

std::string TracksForSend::getAudioMedia(uint16_t port) const
{
    if (!NX_ASSERT(hasAudio()))
        return "";
    return m_session->muxer()->audioEncoder()->getSdpMedia(
        false, 1, port, /*ssl*/ true).toStdString();
}

void TracksForSend::setFallbackCodecs()
{
    m_fallbackCodecs = true;
}

bool TracksForSend::fallbackCodecs()
{
    return m_fallbackCodecs;
}

std::string TracksForSend::mimeType() const
{
    return m_session->muxer()->mimeType();
}

bool TracksForSend::hasVideoTranscoding() const
{
    return m_session->provider()->isTranscodingEnabled(QnAbstractMediaData::VIDEO);
}

bool TracksForSend::hasAudioTranscoding() const
{
    return m_session->provider()->isTranscodingEnabled(QnAbstractMediaData::AUDIO);
}

bool TracksForSend::examineSdp(const std::string& /*sdp*/)
{
    return true;
}

void TracksForSend::addTrackAttributes(const Track& /*track*/, std::string& /*sdp*/) const
{
}

TracksForRecv::TracksForRecv(Session* session): Tracks(session)
{
    setVideoPayloadType(96);
    setAudioPayloadType(0);
}

bool TracksForRecv::hasVideo() const
{
    return true;
}

bool TracksForRecv::hasAudio() const
{
    return m_session->demuxer()->resource()->isAudioEnabled();
}

std::string TracksForRecv::getVideoMedia(uint16_t port) const
{
    return NX_FMT(
        "m=video %1 RTP/AVP %2\r\n"
        "a=rtpmap:%2 H264/90000\r\n"
        "a=fmtp:%2 packetization-mode=1\r\n",
        port,
        videoTrack().payloadType)
        .toStdString();
}

std::string TracksForRecv::getAudioMedia(uint16_t port) const
{
    return NX_FMT(
        "m=audio %1 RTP/AVP %2\r\n"
        "a=rtpmap:%2 PCMU/8000\r\n",
        port,
        audioTrack().payloadType)
        .toStdString();
}

void TracksForRecv::setFallbackCodecs()
{
}

bool TracksForRecv::fallbackCodecs()
{
    return true;
}

std::string TracksForRecv::mimeType() const
{
    return "";
}

bool TracksForRecv::hasVideoTranscoding() const
{
    return false;
}

bool TracksForRecv::hasAudioTranscoding() const
{
    return false;
}

bool TracksForRecv::examineSdp(const std::string& sdp)
{
    m_session->demuxer()->setSdp(sdp);
    if (hasVideo() != m_session->demuxer()->hasVideo()
        || hasAudio() != m_session->demuxer()->hasAudio())
    {
        return false;
    }
    if (hasAudio())
    {
        auto audioCodecParameters = m_session->demuxer()->audioCodecParameters();
        NX_ASSERT(audioCodecParameters);
        auto layout = std::make_shared<AudioLayout>(audioCodecParameters);
        m_session->reader()->setAudioLayout(layout);
    }
    return true;
}

void TracksForRecv::addTrackAttributes(const Track& track, std::string& sdp) const
{
    // Full intra request: https://datatracker.ietf.org/doc/html/rfc5104#section-4.3.1
    // `ccm` mean `Codec Control Message`.
    sdp += "a=rtcp-fb:" + std::to_string(track.payloadType) + " ccm fir\r\n";
}

} // namespace nx::webrtc
