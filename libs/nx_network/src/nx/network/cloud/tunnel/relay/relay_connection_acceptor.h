// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/network/abstract_stream_socket_acceptor.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_notifications.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/reverse_connection_acceptor.h>
#include <nx/network/socket_common.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>

#include "api/relay_api_client.h"
#include "../../cloud_abstract_connection_acceptor.h"

namespace nx::network::cloud::relay {

namespace detail {

class NX_NETWORK_API ReverseConnection:
    public aio::BasicPollable,
    public AbstractAcceptableReverseConnection
{
    using base_type = aio::BasicPollable;

public:
    ReverseConnection(
        const utils::Url& relayUrl,
        std::optional<std::chrono::milliseconds> connectTimeout,
        std::optional<int> forcedHttpTunnelType);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    void connect(
        ReverseConnectionCompletionHandler handler);

    virtual void waitForOriginatorToStartUsingConnection(
        ReverseConnectionCompletionHandler handler) override;

    nx::cloud::relay::api::BeginListeningResponse beginListeningResponse() const;
    std::unique_ptr<AbstractStreamSocket> takeSocket();
    void setTimeout(std::optional<std::chrono::milliseconds> timeout);

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<nx::cloud::relay::api::Client> m_relayClient;
    const std::string m_peerName;
    ReverseConnectionCompletionHandler m_connectHandler;
    std::unique_ptr<nx::network::http::AsyncMessagePipeline> m_httpPipeline;
    ReverseConnectionCompletionHandler m_onConnectionActivated;
    std::unique_ptr<AbstractStreamSocket> m_streamSocket;
    nx::cloud::relay::api::BeginListeningResponse m_beginListeningResponse;

    void onConnectionClosed(SystemError::ErrorCode closeReason, bool /*connectionDestroyed*/);

    void onConnectDone(
        nx::cloud::relay::api::ResultCode resultCode,
        nx::cloud::relay::api::BeginListeningResponse response,
        std::unique_ptr<AbstractStreamSocket> streamSocket);

    void dispatchRelayNotificationReceived(nx::network::http::Message message);

    void processOpenTunnelNotification(
        nx::cloud::relay::api::OpenTunnelNotification openTunnelNotification);
};

//-------------------------------------------------------------------------------------------------

/**
 * Opens server-side connections to the relay.
 */
class NX_NETWORK_API ReverseConnector:
    public SimpleReverseConnector<ReverseConnection>
{
    using base_type = SimpleReverseConnector<ReverseConnection>;

public:
    ReverseConnector(const nx::utils::Url& relayUrl);

    /**
     * Note: it is possible that a single call to this method will actually open multiple connections
     * to the relay.
     * The reason is that there can be multiple HTTP tunnel types with the same priority.
     * And, in this case, all succeeded tunnels are provided.
     */
    virtual void connectAsync() override;

    void setConnectTimeout(std::optional<std::chrono::milliseconds> timeout);

private:
    nx::utils::Url m_relayUrl;
    std::optional<std::chrono::milliseconds> m_connectTimeout;
};

} // namespace detail

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API ConnectionAcceptor:
    public AbstractConnectionAcceptor
{
    using base_type = AbstractConnectionAcceptor;

public:
    ConnectionAcceptor(const utils::Url& relayUrl);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual void cancelIOSync() override;

    virtual std::unique_ptr<AbstractStreamSocket> getNextSocketIfAny() override;

    void setConnectTimeout(std::optional<std::chrono::milliseconds> timeout);

protected:
    virtual void stopWhileInAioThread() override;

private:
    const nx::utils::Url m_relayUrl;
    ReverseConnectionAcceptor<detail::ReverseConnection> m_acceptor;
    bool m_started = false;

    void updateAcceptorConfiguration(const detail::ReverseConnection& newConnection);

    std::unique_ptr<AbstractStreamSocket> toStreamSocket(
        std::unique_ptr<detail::ReverseConnection> connection);
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API ConnectionAcceptorFactory:
    public nx::utils::BasicFactory<
        std::unique_ptr<AbstractConnectionAcceptor>(const nx::utils::Url& /*relayUrl*/)>
{
    using base_type = nx::utils::BasicFactory<
        std::unique_ptr<AbstractConnectionAcceptor>(const nx::utils::Url& /*relayUrl*/)>;

public:
    ConnectionAcceptorFactory();

    static ConnectionAcceptorFactory& instance();

private:
    std::unique_ptr<AbstractConnectionAcceptor> defaultFactoryFunc(const utils::Url &relayUrl);
};

} // namespace nx::network::cloud::relay
