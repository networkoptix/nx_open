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

    class HttpServerConnection
    :
        public nx_api::BaseStreamProtocolConnection<
            HttpServerConnection,
            nx_http::HttpStreamSocketServer,
            nx_http::Message,
            nx_http::MessageParser,
            nx_http::MessageSerializer>
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
            std::unique_ptr<AbstractCommunicatingSocket> sock );
        ~HttpServerConnection();

        void processMessage( nx_http::Message&& request );

    private:
        std::unique_ptr<nx_http::AbstractMsgBodySource> m_currentMsgBody;

        void prepareAndSendResponse(
            nx_http::Message&& response,
            std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody );
        void responseSent();
        void someMsgBodyRead( SystemError::ErrorCode, BufferType buf );
        void someMessageBodySent();
    };
}

#endif  //HTTP_SERVER_CONNECTION_H
