#pragma once

#include "message.h"
#include "../abstract_aliveness_tester.h"

namespace nx::network::stun {

class AbstractAsyncClient;

/**
 * Tests STUN server aliveness by sending STUN binding requests
 * (every STUN server MUST support that).
 * Any response is considered to signal server aliveness.
 */
class NX_NETWORK_API ServerAlivenessTester:
    public AbstractAlivenessTester
{
    using base_type = AbstractAlivenessTester;

public:
    /**
     * NOTE: Binds itself to the client's AIO thread.
     */
    ServerAlivenessTester(
        KeepAliveOptions keepAliveOptions,
        AbstractAsyncClient* client);
    ~ServerAlivenessTester();

protected:
    /**
     * Sends STUN binding request. Any response is considered valid.
     */
    virtual void probe(ProbeResultHandler handler) override;

    virtual void cancelProbe() override;

private:
    AbstractAsyncClient* m_client = nullptr;

    void processResponse(
        ProbeResultHandler handler,
        SystemError::ErrorCode resultCode,
        Message message);
};

} // namespace nx::network::stun
