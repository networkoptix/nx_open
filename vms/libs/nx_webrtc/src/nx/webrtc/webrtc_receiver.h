// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "webrtc_ice.h"

namespace nx::webrtc {

class NX_WEBRTC_API Receiver: public AbstractIceDelegate
{
public:
    Receiver(Session* session);

    // From AbstractIceDelegate.
    virtual bool start(Ice* ice, Dtls* dtls) override final;
    virtual void stop() override final;
    virtual bool onSrtp(std::vector<uint8_t> buffer) override final;
    virtual void onDataChannelString(const std::string& data, int streamId) override final;
    virtual void onDataChannelBinary(const std::string& data, int streamId) override final;
    virtual ~Receiver() override;

    // Interface for LiveStreamReader.
    Ice* ice();

private:
    Session* m_session = nullptr;
    Ice* m_ice = nullptr;
};

} // namespace nx::webrtc
