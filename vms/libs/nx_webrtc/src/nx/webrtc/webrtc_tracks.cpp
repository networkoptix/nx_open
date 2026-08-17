// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_tracks.h"

#include <core/resource/camera_resource.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/utils/random.h>
#include <nx/utils/log/log.h>

#include "webrtc_session.h"
#include "webrtc_twcc.h"

namespace nx::webrtc {

namespace {

static const std::string kSendOnlyAttr = "a=sendonly";
static const std::string kRecvOnlyAttr = "a=recvonly";
static const std::string kSendRecvAttr = "a=sendrecv";

// PCMU's statically assigned RTP payload type, RFC 3551 table 4.
constexpr int kPcmuPayloadType = 0;

// One codec offered on an m-line. `channels` is omitted from the rtpmap when zero, and
// `fmtpParams` produces no fmtp line when empty.
struct OfferedCodec
{
    int payloadType = -1;
    std::string name;
    int clockRate = 0;
    int channels = 0;
    std::string fmtpParams;
};

// Builds the m-line plus the rtpmap and optional fmtp lines for every offered codec. The answerer
// picks one of the payload types listed on the m-line (RFC 3264), and their order expresses our
// preference, so pass the codecs most-preferred first.
// Uses the DTLS-SRTP AVPF profile (RFC 4585) so rtcp-fb directives are honored
// -- Safari/iOS enforce this strictly, Chrome is lenient -- and a bundled session
// must not mix AVP and SAVPF across its m-lines.
std::string makeMediaLines(
    const std::string& mediaType, uint16_t port, const std::vector<OfferedCodec>& codecs)
{
    auto mlines = "m=" + mediaType + " " + std::to_string(port) + " UDP/TLS/RTP/SAVPF";
    for (const auto& codec: codecs)
        mlines += " " + std::to_string(codec.payloadType);
    mlines += "\r\n";

    for (const auto& codec: codecs)
    {
        mlines += "a=rtpmap:" + std::to_string(codec.payloadType) + " " + codec.name + "/"
            + std::to_string(codec.clockRate);
        if (codec.channels > 0)
            mlines += "/" + std::to_string(codec.channels);
        mlines += "\r\n";

        if (!codec.fmtpParams.empty())
        {
            mlines +=
                "a=fmtp:" + std::to_string(codec.payloadType) + " " + codec.fmtpParams + "\r\n";
        }
    }

    return mlines;
}

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

uint32_t Tracks::updateSsrc(Track* track, std::optional<uint32_t> newSsrc)
{
    if (!track)
        return 0;

    if (!newSsrc)
    {
        do
            newSsrc = nx::utils::random::number<uint32_t>();
        while (m_tracks.contains(*newSsrc));
    }
    else if (m_tracks.contains(*newSsrc))
    {
        return 0;
    }

    auto it = m_tracks.find(track->ssrc);
    if (!NX_ASSERT(it != m_tracks.end() && it->second.get() == track, "Foreign track"))
        return 0;

    auto ownedTrack = std::move(it->second);
    m_tracks.erase(it);
    ownedTrack->ssrc = *newSsrc;
    m_tracks.emplace(*newSsrc, std::move(ownedTrack));
    return *newSsrc;
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
    if (!m_session->demuxer()->hasVideo()
        || m_session->demuxer()->resource()->isAudioEnabled() != m_session->demuxer()->hasAudio())
    {
        return false;
    }

    if (m_session->demuxer()->resource()->isAudioEnabled())
    {
        auto audioCodecParameters = m_session->demuxer()->audioCodecParameters();
        NX_ASSERT(audioCodecParameters);
        auto layout = std::make_shared<AudioLayout>(audioCodecParameters);
        m_session->reader()->setAudioLayout(layout);
    }
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
    bool isVideo = track->trackType == TrackType::video;
    auto encoder = isVideo
        ? m_session->muxer()->videoEncoder(track->deviceId)
        : m_session->muxer()->audioEncoder(track->deviceId);

    if (!encoder)
    {
        NX_DEBUG(this, "Encoder is not initialized for track %1:%2, type: %3",
            track->deviceId, track->ssrc, (int)track->trackType);
        return {};
    }

    return encoder->getSdpMedia(
        isVideo, track->mid, port, /*ssl*/ true).toStdString()
        + base_type::getSdpForTrack(track, port);
}

std::string TracksForRecv::getSdpForTrack(const Track* track, uint16_t port) const
{
    std::string sdp;
    if (track->trackType == TrackType::video)
    {
        sdp = makeMediaLines("video",
            port,
            {{
                .payloadType = track->payloadType,
                .name = "H264",
                .clockRate = 90000,
                .fmtpParams = "packetization-mode=1",
            }});

        // Full intra request: https://datatracker.ietf.org/doc/html/rfc5104#section-4.3.1
        // `ccm` means `Codec Control Message`.
        sdp += "a=rtcp-fb:" + std::to_string(track->payloadType) + " ccm fir\r\n";

        // Transport-wide congestion control: we echo per-packet arrival timing
        // back to the publisher so its send-side estimator can raise the send
        // bitrate. Advertising only transport-cc (not goog-remb) makes the
        // publisher select send-side BWE deterministically. See VMS-62147.
        sdp += "a=rtcp-fb:" + std::to_string(track->payloadType) + " transport-cc\r\n" +
            "a=extmap:" + std::to_string(kTransportCcExtId) + " " + kTransportCcUri + "\r\n";
    }
    else if (track->trackType == TrackType::audio)
    {
        // Opus first: every WebRTC stack implements it, it is what browsers pick by default, and
        // unlike PCMU it is not locked to 8 kHz narrowband. Per RFC 7587 section 7 the rtpmap
        // always reads 48000/2 regardless of what the sender ends up encoding. `useinbandfec=1`
        // lets the sender add in-band FEC, which matters because we archive this stream and cannot
        // ask for a retransmit later. Stereo is left at its default of mono.
        //
        // PCMU stays on the m-line only as a fallback for a publisher that cannot do Opus.
        // examineSdp() rejects the whole session when the publisher answers with no audio, so
        // offering Opus alone would cost such a publisher its video too, not just its audio. Any
        // browser prefers Opus over G.711, so this costs nothing in practice.
        sdp = makeMediaLines("audio",
            port,
            {
                {
                    .payloadType = track->payloadType,
                    .name = "opus",
                    .clockRate = 48000,
                    .channels = 2,
                    .fmtpParams = "minptime=10;useinbandfec=1",
                },
                {
                    .payloadType = kPcmuPayloadType,
                    .name = "PCMU",
                    .clockRate = 8000,
                },
            });
    }
    else
    {
        NX_DEBUG(this, "Unknown track type %1", (int) track->trackType);
    }

    return sdp + base_type::getSdpForTrack(track, port);
}

TracksForSend::TracksForSend(Session* session) : Tracks(session)
{
}

TracksForRecv::TracksForRecv(Session* session) : Tracks(session)
{
}

} // namespace nx::webrtc
