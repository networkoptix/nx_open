// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_tracks.h"

#include <core/resource/camera_resource.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/utils/random.h>

#include "webrtc_session.h"

namespace nx::webrtc {

namespace {

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

Track* Tracks::videoTrack(nx::Uuid deviceId) const
{
    for (const auto& [_,track]: m_tracks)
    {
        if (track->deviceId == deviceId && track->trackType == TrackType::video)
            return track.get();
    }
    return nullptr;
}
Track* Tracks::audioTrack(nx::Uuid deviceId) const
{
    for (const auto& [_, track]: m_tracks)
    {
        if (track->deviceId == deviceId && track->trackType == TrackType::audio)
            return track.get();
    }
    return nullptr;
}

std::vector<Track> Tracks::allTracks() const
{
    std::vector<Track> result;
    for (const auto& [_, track]: m_tracks)
        result.emplace_back(*track.get());

    std::ranges::sort(
        result,
        [](const auto& t1, const auto& t2)
        {
            return t1.mid < t2.mid;
        });

    return result;
}

Track* Tracks::track(uint32_t ssrc) const
{
    const auto it = m_tracks.find(ssrc);
    return it != m_tracks.end() ? it->second.get() : nullptr;
}

Tracks::Tracks(Session* session)
    : m_session(session)
{
}

void Tracks::addTrack(std::unique_ptr<Track> track)
{
    do {
        track->ssrc = nx::utils::random::number<uint32_t>();
    } while (m_tracks.find(track->ssrc) != m_tracks.end());
    track->purpose = m_session->purpose();

    // Find existing track with same deviceId
    bool found = false;
    for (const auto& [_, t]: m_tracks)
    {
        if (t->deviceId == track->deviceId)
        {
            track->cname = t->cname;
            track->streamId = t->streamId;
            found = true;
            break;
        }
    }
    if (!found)
    {
        track->cname = nx::Uuid::createUuid().toSimpleStdString();
        const nx::Uuid guid = track->deviceId.isNull() ? nx::Uuid::createUuid() : track->deviceId;
        track->streamId = guid.toSimpleStdString();
    }
    track->trackId = nx::Uuid::createUuid().toSimpleStdString();

    for (auto it = m_tracks.begin(); it != m_tracks.end(); ++it)
    {
        const auto& t = it->second;
        if (t->offerState == TrackState::inactive && t->trackType == track->trackType)
        {
            track->mid = t->mid;
            m_tracks.erase(it);
            break;
        }
    }

    if (track->mid == -1)
        track->mid = m_lastMid++;
    m_tracks.emplace(track->ssrc, std::move(track));
}

std::string TracksForSend::mimeType() const
{
    return m_session->muxer()->mimeType();
}

std::string TracksForRecv::mimeType() const
{
    return "";
}

bool TracksForSend::examineSdp(const std::string& /*sdp*/)
{
    return true;
}

bool TracksForRecv::examineSdp(const std::string& sdp)
{
    m_session->demuxer()->setSdp(sdp);

#if 0
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
#endif

    return true;
}

std::string Tracks::getSdpForTrack(const Track* track, uint16_t /*port*/) const
{
    std::string sdp;
#define ENDL "\r\n";
    // Used with STUN handshake
    // Media ID.
    sdp += "a=mid:" + std::to_string(track->mid) + ENDL;
    /* https://datatracker.ietf.org/doc/html/rfc3264#section-5.1
     * If the offerer wishes to only send media on a stream to its peer, it
     * MUST mark the stream as sendonly with the "a=sendonly" attribute.
     * */
    // Probably unused fields.
    sdp += "a=msid:" + track->streamId + " " + track->trackId + ENDL;
    if (track->offerState == TrackState::active)
    {
        sdp += toSdpAttribute(track->purpose) + ENDL;
    }
    else
    {
        sdp += "a=inactive" ENDL;
    }
    // Mux rtcp and rtp stream.
    sdp += "a=rtcp-mux" ENDL;
    /* 'actpass' due to bug in old Chromium. Actually, for incoming connection, should be 'passive':
     * https://datatracker.ietf.org/doc/html/rfc4145#section-4 */
    sdp += "a=setup:actpass" ENDL;
    // 'ssrc' of track used by Chromium's demuxer. Not sure about 'cname'.
    sdp += "a=ssrc:" + std::to_string(track->ssrc) + " cname:" + track->cname + ENDL;
    // Generic RTCP feedbacks supported: https://www.rfc-editor.org/rfc/rfc4585.html#section-3.6.2
    sdp += "a=rtcp-fb:" + std::to_string(track->payloadType) + " nack" ENDL;

    // Mid in RTP extension is not used now.
    //sdp += "a=extmap:1 urn:ietf:params:rtp-hdrext:sdes:mid" ENDL;
    return sdp;
}

std::string TracksForSend::getSdpForTrack(const Track* track, uint16_t port) const
{
    auto encoder = m_session->muxer()->videoEncoder(track->deviceId);
    if (!NX_ASSERT(encoder))
        return {};
    bool isVideo = track->trackType == TrackType::video;
    return encoder->getSdpMedia(
        isVideo, track->mid, port, /*ssl*/ true).toStdString()
        + base_type::getSdpForTrack(track, port);
}

std::string TracksForRecv::getSdpForTrack(const Track* track, uint16_t port) const
{
    std::string result;
    if (track->trackType == TrackType::video)
    {
        result = NX_FMT(
            "m=video %1 RTP/AVP %2\r\n"
            "a=rtpmap:%2 H264/90000\r\n"
            "a=fmtp:%2 packetization-mode=1\r\n",
            port,
            track->payloadType)
            .toStdString();
    }
    else
    {
        result = NX_FMT(
            "m=audio %1 RTP/AVP %2\r\n"
            "a=rtpmap:%2 PCMU/8000\r\n",
            port,
            track->payloadType)
            .toStdString();
    }
    // Full intra request: https://datatracker.ietf.org/doc/html/rfc5104#section-4.3.1
    // `ccm` mean `Codec Control Message`.
    result += "a=rtcp-fb:" + std::to_string(track->payloadType) + " ccm fir\r\n";
    return result + base_type::getSdpForTrack(track, port);
}

TracksForSend::TracksForSend(Session* session) : Tracks(session)
{
}

TracksForRecv::TracksForRecv(Session* session) : Tracks(session)
{
}

} // namespace nx::webrtc
