// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mediator_connector.h"

#include <nx/network/socket_factory.h>
#include <nx/network/stun/client_connection_validator.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "connection_mediator_url_fetcher.h"
#include "mediator/api/mediator_api_http_paths.h"
#include "mediator_endpoint_provider.h"
#include "mediator_stun_client.h"

namespace nx::hpm::api {

namespace { static network::stun::AbstractAsyncClient::Settings s_stunClientSettings; }

MediatorConnector::MediatorConnector(const std::string& cloudHost):
    m_mediatorEndpointProvider(
        std::make_unique<MediatorEndpointProvider>(cloudHost))
{
    m_stunClient = std::make_shared<MediatorStunClient>(
        s_stunClientSettings,
        m_mediatorEndpointProvider.get());

    bindToAioThread(getAioThread());
}

MediatorConnector::~MediatorConnector()
{
    NX_ASSERT((m_stunClient == nullptr) || m_stunClient.use_count() == 1);
    pleaseStopSync();
}

void MediatorConnector::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    network::aio::BasicPollable::bindToAioThread(aioThread);

    m_stunClient->bindToAioThread(aioThread);
    if (m_mediatorEndpointProvider)
        m_mediatorEndpointProvider->bindToAioThread(aioThread);
}

std::unique_ptr<MediatorClientTcpConnection> MediatorConnector::clientConnection()
{
    return std::make_unique<MediatorClientTcpConnection>(m_stunClient);
}

std::unique_ptr<MediatorServerTcpConnection> MediatorConnector::systemConnection()
{
    return std::make_unique<MediatorServerTcpConnection>(m_stunClient, this);
}

void MediatorConnector::mockupCloudModulesXmlUrl(
    const nx::utils::Url& cloudModulesXmlUrl)
{
    m_mediatorEndpointProvider->mockupCloudModulesXmlUrl(cloudModulesXmlUrl);
}

void MediatorConnector::mockupMediatorAddress(
    const MediatorAddress& mediatorAddress)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (const auto currentMediatorAddress = m_mediatorEndpointProvider->mediatorAddress();
            currentMediatorAddress && mediatorAddress == *currentMediatorAddress)
        {
            return;
        }

        NX_DEBUG(this, nx::format("Mediator address is mocked up: %1")
            .arg(mediatorAddress));

        m_mockedUpMediatorAddress = mediatorAddress;
        m_mediatorEndpointProvider->mockupMediatorAddress(mediatorAddress);
    }

    establishTcpConnectionToMediatorAsync();
}

void MediatorConnector::setSystemCredentials(std::optional<SystemCredentials> value)
{
    bool needToReconnect = false;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_credentials == value)
            return;

        needToReconnect = static_cast<bool>(m_credentials);
        m_credentials = std::move(value);
        m_stunClient->setCredentials(m_credentials);
    }

    if (needToReconnect)
        m_stunClient->closeConnection(SystemError::connectionReset);

    m_credentialsSetEvent.notify(m_credentials);
}

std::optional<SystemCredentials> MediatorConnector::getSystemCredentials() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_credentials;
}

void MediatorConnector::fetchAddress(
    nx::utils::MoveOnlyFunc<void(
        nx::network::http::StatusCode::Value /*resultCode*/,
        MediatorAddress /*address*/)> handler)
{
    post(
        [this, handler = std::move(handler)]() mutable
        {
            if (const auto address = m_mediatorEndpointProvider->mediatorAddress())
                return handler(network::http::StatusCode::ok, *address);

            m_mediatorEndpointProvider->fetchMediatorEndpoints(
                [this, handler = std::move(handler)](
                    nx::network::http::StatusCode::Value resultCode)
                {
                    if (nx::network::http::StatusCode::isSuccessCode(resultCode))
                        handler(resultCode, *m_mediatorEndpointProvider->mediatorAddress());
                    else
                        handler(resultCode, MediatorAddress());
                });
        });
}

std::optional<MediatorAddress> MediatorConnector::address() const
{
    return m_mediatorEndpointProvider->mediatorAddress();
}

void MediatorConnector::subsribeToSystemCredentialsSet(
    nx::utils::MoveOnlyFunc<void(std::optional<SystemCredentials>)> handler,
    nx::utils::SubscriptionId* outId)
{
    m_credentialsSetEvent.subscribe(std::move(handler), outId);
}

void MediatorConnector::unsubscribeFromSystemCredentialsSet(nx::utils::SubscriptionId id)
{
    m_credentialsSetEvent.removeSubscription(id);
}

void MediatorConnector::setOnConnectionClosedHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_stunClient->setOnConnectionClosedHandler(std::move(handler));
}

network::stun::AbstractAsyncClient::Settings
    MediatorConnector::setStunClientSettings(
        network::stun::AbstractAsyncClient::Settings stunClientSettings)
{
    return std::exchange(s_stunClientSettings, std::move(stunClientSettings));
}

void MediatorConnector::stopWhileInAioThread()
{
    m_stunClient.reset();
    if (m_mediatorEndpointProvider)
        m_mediatorEndpointProvider->pleaseStopSync();
}

void MediatorConnector::establishTcpConnectionToMediatorAsync()
{
    NX_ASSERT(m_mediatorEndpointProvider->mediatorAddress());

    auto createStunTunnelUrl =
        nx::network::url::Builder(m_mediatorEndpointProvider->mediatorAddress()->tcpUrl)
            .appendPath(api::kStunOverHttpTunnelPath).toUrl();

    m_stunClient->connect(
        createStunTunnelUrl,
        [this, createStunTunnelUrl](SystemError::ErrorCode code)
        {
            if (code != SystemError::noError)
            {
                NX_DEBUG(this, nx::format("Failed to connect to mediator on %1. %2")
                    .args(createStunTunnelUrl, SystemError::toString(code)));
            }
        });
}

} // namespace nx::hpm::api
