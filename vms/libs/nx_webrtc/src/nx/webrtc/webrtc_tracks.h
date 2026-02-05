// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "webrtc_utils.h"

namespace nx::webrtc {

struct NX_WEBRTC_API Track
{
    uint32_t ssrc = 0;
    std::string cname;
    std::string msid;
    std::string mid;
    int payloadType = 0;
    Purpose purpose = Purpose::sendrecv;
};

const std::string& toSdpAttribute(Purpose purpose);

class NX_WEBRTC_API Tracks
{
public:
    Tracks(Session* session);
    virtual ~Tracks() = default;

    const Track& videoTrack() const { return m_localVideoTrack; }
    const Track& audioTrack() const { return m_localAudioTrack; }
    void setVideoPayloadType(int payloadType);
    void setAudioPayloadType(int payloadType);

    virtual bool hasVideo() const = 0;
    virtual bool hasAudio() const = 0;
    virtual std::string getVideoMedia(uint16_t port) const = 0;
    virtual std::string getAudioMedia(uint16_t port) const = 0;
    virtual void setFallbackCodecs() = 0;
    virtual bool fallbackCodecs() = 0;
    virtual std::string mimeType() const = 0;
    virtual bool hasVideoTranscoding() const = 0;
    virtual bool hasAudioTranscoding() const = 0;
    virtual bool examineSdp(const std::string& sdp) = 0;
    virtual void addTrackAttributes(const Track& track, std::string& sdp) const = 0;

protected:
    Session* m_session = nullptr;

private:
    Track m_localVideoTrack;
    Track m_localAudioTrack;
};

class NX_WEBRTC_API TracksForSend: public Tracks
{
public:
    TracksForSend(Session* session);
    virtual ~TracksForSend() override = default;

    virtual bool hasVideo() const override;
    virtual bool hasAudio() const override;
    virtual std::string getVideoMedia(uint16_t port) const override;
    virtual std::string getAudioMedia(uint16_t port) const override;
    virtual void setFallbackCodecs() override;
    virtual bool fallbackCodecs() override;
    virtual std::string mimeType() const override;
    virtual bool hasVideoTranscoding() const override;
    virtual bool hasAudioTranscoding() const override;
    virtual bool examineSdp(const std::string& sdp) override;
    virtual void addTrackAttributes(const Track& track, std::string& sdp) const override;

private:
    bool m_fallbackCodecs = false;
};

class NX_WEBRTC_API TracksForRecv: public Tracks
{
public:
    TracksForRecv(Session* session);
    virtual ~TracksForRecv() override = default;

    virtual bool hasVideo() const override;
    virtual bool hasAudio() const override;
    virtual std::string getVideoMedia(uint16_t port) const override;
    virtual std::string getAudioMedia(uint16_t port) const override;
    virtual void setFallbackCodecs() override;
    virtual bool fallbackCodecs() override;

    virtual std::string mimeType() const override;
    virtual bool hasVideoTranscoding() const override;
    virtual bool hasAudioTranscoding() const override;

    virtual bool examineSdp(const std::string& sdp) override;
    virtual void addTrackAttributes(const Track& track, std::string& sdp) const override;
};

} // namespace nx::webrtc
