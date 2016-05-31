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


namespace stree
{
    class ResourceContainer;
}   //namespace stree

namespace nx_http
{
    class AbstractAuthenticationManager;
    class MessageDispatcher;

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

    class NX_NETWORK_API HttpServerConnection
    :
        public BaseConnection<HttpServerConnection>,
        public std::enable_shared_from_this<HttpServerConnection>
    {
    public:
        typedef BaseConnection<HttpServerConnection> BaseType;

        HttpServerConnection(
            StreamConnectionHolder<HttpServerConnection>* socketServer,
            std::unique_ptr<AbstractCommunicatingSocket> sock,
            nx_http::AbstractAuthenticationManager* const authenticationManager,
            nx_http::MessageDispatcher* const httpMessageDispatcher);
        virtual ~HttpServerConnection();

        virtual void pleaseStop(
            nx::utils::MoveOnlyFunc<void()> completionHandler) override;

        void processMessage(nx_http::Message&& request);

    private:
        struct ResponseContext
        {
            nx_http::Message msg;
            std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody;

            ResponseContext(
                nx_http::Message _msg,
                std::unique_ptr<nx_http::AbstractMsgBodySource> _responseMsgBody)
            :
                msg(std::move(_msg)),
                responseMsgBody(std::move(_responseMsgBody))
            {
            }
        };

        nx_http::AbstractAuthenticationManager* const m_authenticationManager;
        nx_http::MessageDispatcher* const m_httpMessageDispatcher;
        std::unique_ptr<nx_http::AbstractMsgBodySource> m_currentMsgBody;
        bool m_isPersistent;
        std::deque<ResponseContext> m_responseQueue;

        bool authenticateRequest(
            const nx_http::Request& request,
            stree::ResourceContainer* const authInfo);
        void prepareAndSendResponse(
            nx_http::MimeProtoVersion version,
            nx_http::Message&& response,
            std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody );
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
