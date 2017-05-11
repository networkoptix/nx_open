#pragma once

#include <memory>

#include <QtCore/QUrl>

#include <nx/network/abstract_acceptor.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/reverse_connection_acceptor.h>
#include <nx/network/socket_common.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>

#include "api/relay_api_client.h"
#include "../../cloud_abstract_connection_acceptor.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {

namespace detail {

class NX_NETWORK_API ReverseConnection:
    public aio::BasicPollable,
    public AbstractAcceptableReverseConnection,
    public server::StreamConnectionHolder<nx_http::AsyncMessagePipeline>
{
    using base_type = aio::BasicPollable;

public:
    ReverseConnection(const QUrl& relayUrl);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void connectToOriginator(
        ReverseConnectionCompletionHandler handler) override;
    virtual void waitForOriginatorToStartUsingConnection(
        ReverseConnectionCompletionHandler handler) override;

    std::unique_ptr<AbstractStreamSocket> takeSocket();

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<nx::cloud::relay::api::Client> m_relayClient;
    const nx::String m_peerName;
    ReverseConnectionCompletionHandler m_connectHandler;
    std::unique_ptr<nx_http::AsyncMessagePipeline> m_httpPipeline;
    ReverseConnectionCompletionHandler m_onConnectionActivated;
    std::unique_ptr<AbstractStreamSocket> m_streamSocket;

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        nx_http::AsyncMessagePipeline* connection) override;

    void onConnectDone(
        nx::cloud::relay::api::ResultCode resultCode,
        nx::cloud::relay::api::BeginListeningResponse response,
        std::unique_ptr<AbstractStreamSocket> streamSocket);

    void relayNotificationReceived(nx_http::Message message);
};

} // namespace detail

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API ConnectionAcceptor:
    public AbstractConnectionAcceptor
{
    using base_type = AbstractConnectionAcceptor;

public:
    ConnectionAcceptor(const QUrl& relayUrl);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual void cancelIOSync() override;

    virtual std::unique_ptr<AbstractStreamSocket> getNextSocketIfAny() override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const QUrl m_relayUrl;
    ReverseConnectionAcceptor<detail::ReverseConnection> m_acceptor;

    std::unique_ptr<detail::ReverseConnection> reverseConnectionFactoryFunc();

    std::unique_ptr<AbstractStreamSocket> toStreamSocket(
        std::unique_ptr<detail::ReverseConnection> connection);
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API ConnectionAcceptorFactory:
    public nx::utils::BasicFactory<
        std::unique_ptr<AbstractConnectionAcceptor>(const QUrl& /*relayUrl*/)>
{
    using base_type = nx::utils::BasicFactory<
        std::unique_ptr<AbstractConnectionAcceptor>(const QUrl& /*relayUrl*/)>;

public:
    ConnectionAcceptorFactory();

    static ConnectionAcceptorFactory& instance();

private:
    std::unique_ptr<AbstractConnectionAcceptor> defaultFactoryFunc(
        const QUrl& relayUrl);
};

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
