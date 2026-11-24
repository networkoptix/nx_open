// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "webrtc_ice.h"
#include "webrtc_reader.h"

namespace nx::webrtc {

class NX_WEBRTC_API Receiver: public AbstractIceDelegate
{
public:
    Receiver(Session* session, std::shared_ptr<Reader> reader);

    // From AbstractIceDelegate.
    virtual bool start(Ice* ice, Dtls* dtls) override final;
    virtual void stop() override final;
    virtual bool onSrtp(std::vector<uint8_t> buffer) override final;
    virtual void onDataChannelString(const std::string& data, int streamId) override final;
    virtual void onDataChannelBinary(const std::string& data, int streamId) override final;
    virtual ~Receiver() override;

    // Interface for LiveStreamReader.
    Ice* ice();

    Reader* reader() const { return m_reader.get(); }

private:
    Session* m_session = nullptr;
    Ice* m_ice = nullptr;
    const std::shared_ptr<Reader> m_reader;
};

} // namespace nx::webrtc
