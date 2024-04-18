// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/retry_timer.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/stun/async_client_delegate.h>
#include <nx/network/stun/async_client_with_http_tunneling.h>
#include <nx/utils/std/future.h>
#include <nx/utils/subscription.h>

#include "abstract_cloud_system_credentials_provider.h"
#include "mediator_client_connections.h"
#include "mediator_endpoint_provider.h"
#include "mediator_server_connections.h"

namespace nx::network::cloud { class ConnectionMediatorUrlFetcher; }

namespace nx::hpm::api {

class MediatorStunClient;
class MediatorEndpointProvider;

using MediatorAvailabilityChangedHandler =
    nx::utils::MoveOnlyFunc<void(bool /*isMediatorAvailable*/)>;

class NX_NETWORK_API AbstractMediatorConnector:
    public network::aio::BasicPollable
{
public:
    virtual ~AbstractMediatorConnector() = default;

    /** Provides client related functionality. */
    virtual std::unique_ptr<MediatorClientTcpConnection> clientConnection() = 0;

    /** Provides server-related functionality. */
    virtual std::unique_ptr<MediatorServerTcpConnection> systemConnection() = 0;

    /**
     * @param endpoint Defined only if resultCode is SystemError::noError.
     */
    virtual void fetchAddress(nx::utils::MoveOnlyFunc<void(
        nx::network::http::StatusCode::Value /*resultCode*/,
        MediatorAddress /*address*/)> handler) = 0;

    /**
     * Provides endpoint without blocking.
     */
    virtual std::optional<MediatorAddress> address() const = 0;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API MediatorConnector:
    public AbstractMediatorConnector,
    public AbstractCloudSystemCredentialsProvider
{
public:
    MediatorConnector(const std::string& cloudHost);
    virtual ~MediatorConnector() override;

    MediatorConnector(MediatorConnector&&) = delete;
    MediatorConnector& operator=(MediatorConnector&&) = delete;

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    /** Provides client related functionality */
    virtual std::unique_ptr<MediatorClientTcpConnection> clientConnection() override;

    /** Provides system related functionality */
    virtual std::unique_ptr<MediatorServerTcpConnection> systemConnection() override;

    /**
     * NOTE: Mediator url resolution will still happen by referring to specified address.
     */
    void mockupCloudModulesXmlUrl(const utils::Url &cloudModulesXmlUrl);

    /**
     * Injects mediator url.
     * As a result, no mediator url resolution will happen.
     */
    void mockupMediatorAddress(const MediatorAddress& mediatorAddress);

    void setSystemCredentials(std::optional<SystemCredentials> value);

    virtual std::optional<SystemCredentials> getSystemCredentials() const override;

    virtual void fetchAddress(nx::utils::MoveOnlyFunc<void(
        nx::network::http::StatusCode::Value /*resultCode*/,
        MediatorAddress /*address*/)> handler) override;

    virtual std::optional<MediatorAddress> address() const override;

    void subsribeToSystemCredentialsSet(
        nx::utils::MoveOnlyFunc<void(std::optional<SystemCredentials>)> handler,
        nx::utils::SubscriptionId* outId);

    void unsubscribeFromSystemCredentialsSet(nx::utils::SubscriptionId id);

    /**
     * @param handler Will be called when connection to the mediator is terminated for any reason.
     * Depending on settings, a reconnect attempt could be scheduled automatically. So, generally,
     * there is no need to response to this event.
     */
    void setOnConnectionClosedHandler(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler);

    /**
     * @return The previous settings.
     */
    static network::stun::AbstractAsyncClient::Settings setStunClientSettings(
        network::stun::AbstractAsyncClient::Settings stunClientSettings);

private:
    mutable nx::Mutex m_mutex;
    std::optional<SystemCredentials> m_credentials;
    nx::utils::Subscription<std::optional<SystemCredentials>> m_credentialsSetEvent;

    std::unique_ptr<MediatorEndpointProvider> m_mediatorEndpointProvider;
    std::shared_ptr<MediatorStunClient> m_stunClient;
    std::optional<MediatorAddress> m_mockedUpMediatorAddress;

    virtual void stopWhileInAioThread() override;

    void establishTcpConnectionToMediatorAsync();
};

} // namespace nx::hpm::api
