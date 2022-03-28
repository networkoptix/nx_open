// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <vector>

#include <nx/network/aio/async_channel_bridge.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/connection_server/detail/connection_statistics.h>
#include <nx/utils/interruption_flag.h>

namespace nx::cloud::gateway {

class BridgeToServerConnectionAdaptor:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    using OnConnectionClosedHandler = nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode /*closeReason*/, bool /*connectionDestroyed*/)>;

    BridgeToServerConnectionAdaptor(std::unique_ptr<network::aio::AsyncChannelBridge> bridge);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    void registerCloseHandler(OnConnectionClosedHandler handler);

    void start();

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<network::aio::AsyncChannelBridge> m_bridge;
    std::vector<OnConnectionClosedHandler> m_connectionClosedHandlers;
    nx::utils::InterruptionFlag m_connectionFreedFlag;

    void triggerConnectionClosedEvent(SystemError::ErrorCode closeReason);
};

} // namespace nx::cloud::gateway
