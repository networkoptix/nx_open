// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "webrtc_utils.h"

#include <optional>

#include <nx/utils/uuid.h>

namespace nx::webrtc {

enum class TrackType {
    unknown,
    video,
    audio
};

struct NX_WEBRTC_API Track
{
    uint32_t ssrc = 0;
    std::string cname;
    std::string streamId;
    std::string trackId;
    int mid = -1;
    int payloadType = 0;
    Purpose purpose = Purpose::sendrecv;
    nx::Uuid deviceId;
    TrackType trackType = TrackType::unknown;

    TrackState offerState{};
    // A renegotiation offer was sent (Session::renegotiateProvider()) but not answered yet, so the
    // client doesn't know the new SSRC/payload type.
    bool pendingAnswer = false;
    // Valid only while pendingAnswer: where to resume the provider (DATETIME_NOW for live).
    int64_t resumePositionUs = 0;

    // No Device holds this m-line. It is still offered, as a rejected one, and the next Device
    // that needs an m-line of this type takes it, see Tracks::addTrack().
    bool isFree() const { return deviceId.isNull() && offerState == TrackState::inactive; }
};

const std::string& toSdpAttribute(Purpose purpose);

class NX_WEBRTC_API Tracks
{
    Tracks(const Tracks&) = delete;
    Tracks& operator=(const Tracks&) = delete;

public:
    // Puts `track` on a free m-line of its type, or on a new one. An m-line the client has seen
    // is never taken away: a later offer can gain m-lines, but cannot lose or reorder them.
    void addTrack(std::unique_ptr<Track> track);

    // Hands the m-lines of `deviceId` back. They stay in the offer, rejected, until addTrack()
    // gives one to another Device.
    void freeTracks(nx::Uuid deviceId);

    // Hands one m-line back, leaving the other m-lines of its Device alone: that Device goes on
    // streaming whatever the client took of it. See freeTracks().
    void freeTrack(Track* track);

    // Without `newSsrc` assigns a fresh random one, telling the receiver that the RTP stream
    // restarted, e.g. on a mid-session codec change. `newSsrc` can be set to roll back to the old
    // ssrc when the codec change fails. Returns the ssrc in use, or 0 if `newSsrc` is taken.
    uint32_t updateSsrc(Track* track, std::optional<uint32_t> newSsrc = std::nullopt);

    Tracks(Session* session);
    virtual ~Tracks() = default;

    Track* videoTrack(nx::Uuid deviceId) const;
    Track* audioTrack(nx::Uuid deviceId) const;
    Track* track(uint32_t ssrc) const;
    // Every m-line of the session, keyed and ordered by its index in the offer.
    std::map<int, std::unique_ptr<Track>>& tracks() { return m_tracks; }

    virtual std::string mimeType() const = 0;
    virtual bool examineSdp(const std::string& sdp) = 0;

    // The media description of `track`, or {} when nothing backs it any more -- then the m-line
    // is offered as a rejected one, see rejectedSdpForTrack().
    virtual std::string getSdpForTrack(const Track* track, uint16_t port) const;

    // An m-line with no media to describe, offered with port 0 so that the client keeps seeing
    // it: rejected, and out of the BUNDLE group (RFC 3264 section 8.2, RFC 8843 section 7.3.3).
    std::string rejectedSdpForTrack(const Track* track) const;

protected:
    Session* m_session = nullptr;

private:
    Track* deviceTrack(nx::Uuid deviceId, TrackType trackType) const;
    bool isSsrcTaken(uint32_t ssrc) const;
    int nextMid();

private:
    // Keyed by the index of the m-line in the offer, which a slot holds for the whole session.
    // What identifies it to the client, the mid, is a field of the track: a slot handed to
    // another Device is a transceiver the client has not seen, and carries a new mid.
    std::map<int, std::unique_ptr<Track>> m_tracks;
    int m_nextMid = 0;
};

class NX_WEBRTC_API TracksForSend: public Tracks
{
    using base_type = Tracks;
public:
    TracksForSend(Session* session);
    virtual ~TracksForSend() override = default;

    virtual std::string mimeType() const override;
    virtual bool examineSdp(const std::string& sdp) override;
    virtual std::string getSdpForTrack(const Track* track, uint16_t port) const override;
};

class NX_WEBRTC_API TracksForRecv: public Tracks
{
    using base_type = Tracks;
public:
    TracksForRecv(Session* session);
    virtual ~TracksForRecv() override = default;

    virtual std::string mimeType() const override;
    virtual bool examineSdp(const std::string& sdp) override;
    virtual std::string getSdpForTrack(const Track* track, uint16_t port) const override;
};

} // namespace nx::webrtc
