// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/cloud/mediator_server_connections.h>
#include <nx/utils/timer_manager.h>

#include "abstract_cloud_system_credentials_provider.h"

namespace nx::network::cloud {

class NX_NETWORK_API MediatorAddressPublisher:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    static const std::chrono::milliseconds kDefaultRetryInterval;

    MediatorAddressPublisher(
        std::unique_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection,
        hpm::api::AbstractCloudSystemCredentialsProvider* cloudCredentialsProvider);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    void setRetryInterval(std::chrono::milliseconds interval);

    void updateAddresses(
        std::vector<SocketAddress> addresses,
        nx::utils::MoveOnlyFunc<void(nx::hpm::api::ResultCode)> updateHandler = nullptr);

private:
    void publishAddressesIfNeeded();
    void registerAddressesOnMediator();
    void scheduleRetry(std::chrono::milliseconds delay);

private:
    std::chrono::milliseconds m_retryInterval;
    bool m_isRequestInProgress;
    std::vector<SocketAddress> m_serverAddresses;
    std::vector<SocketAddress> m_publishedAddresses;
    std::unique_ptr<hpm::api::MediatorServerTcpConnection> m_mediatorConnection;
    std::list<nx::utils::MoveOnlyFunc<void(nx::hpm::api::ResultCode)>> m_updateHandlers;
    std::unique_ptr<aio::Timer> m_retryTimer;
    hpm::api::AbstractCloudSystemCredentialsProvider* m_cloudCredentialsProvider = nullptr;

    virtual void stopWhileInAioThread() override;

    void reportResultToTheCaller(hpm::api::ResultCode resultCode);
};

} // namespace nx::network::cloud
