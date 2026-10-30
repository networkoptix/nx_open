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
    TrackState answerState{};
    // The mid was inherited from an m-line the client has already seen, because addTrack()
    // recycles the mid of a retired track. Set there only, and read only by removeTracks(),
    // which keeps such a track to hold its mid in the recycling pool.
    bool midWasOffered = false;
    // A renegotiation offer was sent (Session::renegotiateProvider()) but not answered yet, so the
    // client doesn't know the new SSRC/payload type.
    bool pendingAnswer = false;
    // Valid only while pendingAnswer: where to resume the provider (DATETIME_NOW for live).
    int64_t resumePositionUs = 0;
};

const std::string& toSdpAttribute(Purpose purpose);

class NX_WEBRTC_API Tracks
{
    Tracks(const Tracks&) = delete;
    Tracks& operator=(const Tracks&) = delete;

public:

    void addTrack(std::unique_ptr<Track> track);

    // Drops the tracks of `deviceId`, for a Device that never reached an offer. A track that
    // recycled the mid of a retired m-line keeps its place in the recycling pool instead, as a
    // Device-less inactive one.
    // NOTE: With no encoder behind it, such a track renders no m-line, so the next offer is one
    // m-line shorter than the one the client accepted. Rare enough to live with: it takes a
    // removed Device, a recycled mid, and a Device that fails to initialize.
    void removeTracks(nx::Uuid deviceId);

    // Without `newSsrc` assigns a fresh random one, telling the receiver that the RTP stream
    // restarted, e.g. on a mid-session codec change. `newSsrc` can be set to roll back to the old
    // ssrc when the codec change fails. Returns the ssrc in use, or 0 if `newSsrc` is taken.
    uint32_t updateSsrc(Track* track, std::optional<uint32_t> newSsrc = std::nullopt);

    Tracks(Session* session);
    virtual ~Tracks() = default;

    Track* videoTrack(nx::Uuid deviceId) const;
    Track* audioTrack(nx::Uuid deviceId) const;
    Track* track(uint32_t ssrc) const;
    std::vector<Track> allTracks() const;
    std::map<uint32_t, std::unique_ptr<Track>>& tracks() { return m_tracks; }

    virtual std::string mimeType() const = 0;
    virtual bool examineSdp(const std::string& sdp) = 0;

    virtual std::string getSdpForTrack(const Track* track, uint16_t port) const;

protected:
    Session* m_session = nullptr;

private:
    std::map<uint32_t, std::unique_ptr<Track>> m_tracks;
    int m_lastMid = 0;
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
