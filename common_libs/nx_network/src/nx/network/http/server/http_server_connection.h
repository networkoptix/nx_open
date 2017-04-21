/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef HTTP_SERVER_CONNECTION_H
#define HTTP_SERVER_CONNECTION_H

#include <memory>

#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/http/abstract_msg_body_source.h>
#include <nx/network/http/httptypes.h>
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

namespace nx_http {

namespace server {
    class AbstractAuthenticationManager;
} // namespace server

class NX_NETWORK_API HttpServerConnection;
class MessageDispatcher;

/** Used to install handlers on some events on HTTP connection.
    \warning There is no way to remove installed event handler.
        Event handler implementation MUST ensure it does not crash
*/
class ConnectionEvents
{
public:
    nx::utils::MoveOnlyFunc<void(HttpServerConnection*)> onResponseHasBeenSent;
};

struct ResponseMessageContext
{
    nx_http::Message msg;
    std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody;
    ConnectionEvents connectionEvents;

    ResponseMessageContext(
        nx_http::Message msg,
        std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody,
        ConnectionEvents connectionEvents)
    :
        msg(std::move(msg)),
        responseMsgBody(std::move(responseMsgBody)),
        connectionEvents(std::move(connectionEvents))
    {
    }
};

typedef nx::utils::MoveOnlyFunc<void(
    nx_http::Message,
    std::unique_ptr<nx_http::AbstractMsgBodySource>,
    ConnectionEvents)> ResponseIsReadyHandler;

template<typename ConnectionType> using BaseConnection = 
    nx_api::BaseStreamProtocolConnection<
        ConnectionType,
        nx_http::Message,
        nx_http::MessageParser,
        nx_http::MessageSerializer>;

typedef nx_api::BaseStreamProtocolConnectionEmbeddable<
    nx_http::Message,
    nx_http::MessageParser,
    nx_http::MessageSerializer> AsyncMessagePipeline;

class NX_NETWORK_API HttpServerConnection:
    public BaseConnection<HttpServerConnection>,
    public std::enable_shared_from_this<HttpServerConnection>
{
public:
    typedef BaseConnection<HttpServerConnection> BaseType;

    HttpServerConnection(
        StreamConnectionHolder<HttpServerConnection>* socketServer,
        std::unique_ptr<AbstractStreamSocket> sock,
        nx_http::server::AbstractAuthenticationManager* const authenticationManager,
        nx_http::MessageDispatcher* const httpMessageDispatcher);
    virtual ~HttpServerConnection();

    virtual void pleaseStop(
        nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    void processMessage(nx_http::Message&& request);
		
    // used for test purpose
    void setPersistentConnectionEnabled(bool value);

private:
    nx_http::server::AbstractAuthenticationManager* const m_authenticationManager;
    nx_http::MessageDispatcher* const m_httpMessageDispatcher;
    std::unique_ptr<nx_http::AbstractMsgBodySource> m_currentMsgBody;
    bool m_isPersistent;
    bool m_persistentConnectionEnabled;
    std::deque<ResponseMessageContext> m_responseQueue;

    void onAuthenticationDone(
        nx_http::server::AuthenticationResult authenticationResult,
        nx_http::Message requestMessage);
    void sendUnauthorizedResponse(
        const nx_http::MimeProtoVersion& protoVersion,
        boost::optional<header::WWWAuthenticate> wwwAuthenticate,
        nx_http::HttpHeaders responseHeaders,
        std::unique_ptr<AbstractMsgBodySource> msgBody);
    void prepareAndSendResponse(
        nx_http::MimeProtoVersion version,
        nx_http::Message&& response,
        std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody,
        ConnectionEvents connectionEvents = ConnectionEvents());
    void sendNextResponse();
    void responseSent();
    void someMsgBodyRead( SystemError::ErrorCode, BufferType buf );
    void readMoreMessageBodyData();
    void fullMessageHasBeenSent();
    void checkForConnectionPersistency( const Message& request );

    HttpServerConnection( const HttpServerConnection& );
    HttpServerConnection& operator=( const HttpServerConnection& );
};

typedef std::weak_ptr<HttpServerConnection> HttpServerConnectionPtr;
}   //namespace nx_http

#endif  //HTTP_SERVER_CONNECTION_H
