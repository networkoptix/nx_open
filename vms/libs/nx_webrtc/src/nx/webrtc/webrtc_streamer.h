// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "webrtc_ice.h"
#include "webrtc_consumer.h"

namespace nx::webrtc {

class NX_WEBRTC_API Streamer: public AbstractIceDelegate
{
public:
    Streamer(Session* session);

    // From AbstractIceDelegate.
    virtual bool start(Ice* ice, Dtls* dtls) override final;
    virtual void stop() override final;
    virtual bool onSrtp(std::vector<uint8_t> buffer) override final;
    virtual void onDataChannelString(const std::string& data, int streamId) override final;
    virtual void onDataChannelBinary(const std::string& data, int streamId) override final;
    virtual ~Streamer() override;

    // Interface for Consumer.
    Ice* ice();
    void sendTimestamp(int64_t timestampUs, uint32_t rtpTimestamp);
    void handleStreamStatus(
        nx::Uuid deviceId,
        Consumer::StreamStatus result);

protected:
    void updateMetrics(int value);

private:
    Session* m_session = nullptr;
    Ice* m_ice = nullptr;
    IceCandidate::Filter m_iceType = IceCandidate::Filter::None;
};

} // namespace nx::webrtc
