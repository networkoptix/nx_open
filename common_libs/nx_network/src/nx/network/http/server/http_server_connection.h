#pragma once

#include <memory>

#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/http/abstract_msg_body_source.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/http_parser.h>
#include <nx/network/http/http_serializer.h>

#include "abstract_authentication_manager.h"

namespace nx {
namespace utils {
namespace stree {

class ResourceContainer;

} //namespace stree
} //namespace utils
} //namespace nx

namespace nx {
namespace network {
namespace http {

namespace server {

class AbstractAuthenticationManager;

} // namespace server

namespace deprecated {

using AsyncMessagePipeline =
    nx::network::server::BaseStreamProtocolConnectionEmbeddable<
        nx::network::http::Message,
        nx::network::http::deprecated::MessageParser,
        nx::network::http::MessageSerializer>;

} // namespace deprecated

using AsyncMessagePipeline =
    nx::network::server::BaseStreamProtocolConnectionEmbeddable<
        nx::network::http::Message,
        nx::network::http::MessageParser,
        nx::network::http::MessageSerializer>;

class NX_NETWORK_API HttpServerConnection;
class AbstractMessageDispatcher;

/**
 * Used to install handlers on some events on HTTP connection.
 * WARNING: There is no way to remove installed event handler.
 *   Event handler implementation MUST ensure it does not crash.
 */
class ConnectionEvents
{
public:
    nx::utils::MoveOnlyFunc<void(HttpServerConnection*)> onResponseHasBeenSent;
};

using ResponseIsReadyHandler =
    nx::utils::MoveOnlyFunc<void(
        nx::network::http::Message,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource>,
        ConnectionEvents)>;

template<typename ConnectionType> using BaseConnection =
    nx::network::server::BaseStreamProtocolConnection<
        ConnectionType,
        nx::network::http::Message,
        nx::network::http::deprecated::MessageParser,
        nx::network::http::MessageSerializer>;

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API HttpServerConnection:
    public BaseConnection<HttpServerConnection>,
    public std::enable_shared_from_this<HttpServerConnection>
{
    using base_type = BaseConnection<HttpServerConnection>;

public:
    HttpServerConnection(
        nx::network::server::StreamConnectionHolder<HttpServerConnection>* socketServer,
        std::unique_ptr<AbstractStreamSocket> sock,
        nx::network::http::server::AbstractAuthenticationManager* const authenticationManager,
        nx::network::http::AbstractMessageDispatcher* const httpMessageDispatcher);

    HttpServerConnection(const HttpServerConnection&) = delete;
    HttpServerConnection& operator=(const HttpServerConnection&) = delete;

    /**
     * Takes into account following HTTP headers:
     * Forwarded, X-Forwarded-For, X-Forwarded-Port.
     */
    SocketAddress clientEndpoint() const;

    /** Introduced for test purpose. */
    void setPersistentConnectionEnabled(bool value);

protected:
    virtual void processMessage(nx::network::http::Message request) override;
    virtual void stopWhileInAioThread() override;

private:
    struct RequestDescriptor
    {
        nx::network::http::MimeProtoVersion httpVersion;
        nx::network::http::StringType protocolToUpgradeTo;
    };

    struct RequestContext
    {
        nx::network::http::Request request;
        RequestDescriptor descriptor;
    };

    struct ResponseMessageContext
    {
        nx::network::http::Message msg;
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> msgBody;
        ConnectionEvents connectionEvents;

        ResponseMessageContext(
            nx::network::http::Message msg,
            std::unique_ptr<nx::network::http::AbstractMsgBodySource> msgBody,
            ConnectionEvents connectionEvents)
            :
            msg(std::move(msg)),
            msgBody(std::move(msgBody)),
            connectionEvents(std::move(connectionEvents))
        {
        }
    };

    nx::network::http::server::AbstractAuthenticationManager* const m_authenticationManager;
    nx::network::http::AbstractMessageDispatcher* const m_httpMessageDispatcher;
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> m_currentMsgBody;
    bool m_isPersistent;
    bool m_persistentConnectionEnabled;
    std::deque<ResponseMessageContext> m_responseQueue;
    std::optional<SocketAddress> m_clientEndpoint;

    void extractClientEndpoint(const HttpHeaders& headers);
    void extractClientEndpointFromXForwardedHeader(const HttpHeaders& headers);
    void extractClientEndpointFromForwardedHeader(const HttpHeaders& headers);

    void authenticate(std::unique_ptr<RequestContext> requestContext);

    void onAuthenticationDone(
        nx::network::http::server::AuthenticationResult authenticationResult,
        std::unique_ptr<RequestContext> requestContext);

    std::unique_ptr<RequestContext> prepareRequestProcessingContext(
        nx::network::http::Request request);

    void sendUnauthorizedResponse(
        std::unique_ptr<RequestContext> requestContext,
        nx::network::http::server::AuthenticationResult authenticationResult);

    void dispatchRequest(
        std::unique_ptr<RequestContext> requestContext,
        nx::network::http::server::AuthenticationResult authenticationResult);

    void processResponse(
        std::shared_ptr<HttpServerConnection> strongThis,
        RequestDescriptor requestDescriptor,
        ResponseMessageContext responseMessageContext);

    void prepareAndSendResponse(
        RequestDescriptor requestDescriptor,
        ResponseMessageContext responseMessageContext);

    void scheduleResponseDelivery(
        const RequestDescriptor& requestDescriptor,
        ResponseMessageContext responseMessageContext);

    void addResponseHeaders(
        const RequestDescriptor& requestDescriptor,
        nx::network::http::Response* response,
        nx::network::http::AbstractMsgBodySource* responseMsgBody);

    void addMessageBodyHeaders(
        nx::network::http::Response* response,
        nx::network::http::AbstractMsgBodySource* responseMsgBody);

    void sendNextResponse();
    void responseSent();
    void someMsgBodyRead(SystemError::ErrorCode, BufferType buf);
    void readMoreMessageBodyData();
    void fullMessageHasBeenSent();
    void checkForConnectionPersistency(const Request& request);
};

using HttpServerConnectionPtr = std::weak_ptr<HttpServerConnection>;

} // namespace nx
} // namespace network
} // namespace http
