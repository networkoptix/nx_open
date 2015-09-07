/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef HTTP_SERVER_CONNECTION_H
#define HTTP_SERVER_CONNECTION_H

#include <memory>

#include <utils/network/connection_server/base_stream_protocol_connection.h>
#include <utils/network/connection_server/stream_socket_server.h>
#include <utils/network/http/abstract_msg_body_source.h>
#include <utils/network/http/httptypes.h>
#include <utils/network/http/http_parser.h>
#include <utils/network/http/http_serializer.h>


namespace nx_http
{
    class HttpStreamSocketServer;
    class AbstractAuthenticationManager;
    class MessageDispatcher;

    class HttpServerConnection
    :
        public nx_api::BaseStreamProtocolConnection<
            HttpServerConnection,
            nx_http::HttpStreamSocketServer,
            nx_http::Message,
            nx_http::MessageParser,
            nx_http::MessageSerializer>,
        public std::enable_shared_from_this<HttpServerConnection>
    {
    public:
        typedef BaseStreamProtocolConnection<
            HttpServerConnection,
            nx_http::HttpStreamSocketServer,
            nx_http::Message,
            nx_http::MessageParser,
            nx_http::MessageSerializer
        > BaseType;

        HttpServerConnection(
            nx_http::HttpStreamSocketServer* socketServer,
            std::unique_ptr<AbstractCommunicatingSocket> sock,
            nx_http::AbstractAuthenticationManager* const authenticationManager,
            nx_http::MessageDispatcher* const httpMessageDispatcher );
        ~HttpServerConnection();

        void processMessage( nx_http::Message&& request );

    private:
        nx_http::AbstractAuthenticationManager* const m_authenticationManager;
        nx_http::MessageDispatcher* const m_httpMessageDispatcher;
        std::unique_ptr<nx_http::AbstractMsgBodySource> m_currentMsgBody;
        bool m_isPersistent;

        void prepareAndSendResponse(
            nx_http::MimeProtoVersion version,
            nx_http::Message&& response,
            std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody );
        void responseSent();
        void someMsgBodyRead( SystemError::ErrorCode, BufferType buf );
        void someMessageBodySent();
        void fullMessageHasBeenSent();
        void checkForConnectionPersistency( const Message& request );

        HttpServerConnection( const HttpServerConnection& );
        HttpServerConnection& operator=( const HttpServerConnection& );
    };

    typedef std::weak_ptr<HttpServerConnection> HttpServerConnectionPtr;
}

#endif  //HTTP_SERVER_CONNECTION_H
