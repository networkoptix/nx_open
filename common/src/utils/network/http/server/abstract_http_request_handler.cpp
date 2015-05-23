/**********************************************************
* 20 may 2015
* a.kolesnikov
***********************************************************/

#include "abstract_http_request_handler.h"

#include <utils/network/http/server/http_stream_socket_server.h>


namespace nx_http
{
    AbstractHttpRequestHandler::AbstractHttpRequestHandler()
    :
        m_response( nx_http::MessageType::response )
    {
    }

    AbstractHttpRequestHandler::~AbstractHttpRequestHandler()
    {
    }

    bool AbstractHttpRequestHandler::processRequest(
        const nx_http::HttpServerConnection& connection,
        nx_http::Message&& request,
        std::function<
            void(
                nx_http::Message&&,
                std::unique_ptr<nx_http::AbstractMsgBodySource> )
            > completionHandler )
    {
        assert( request.type == nx_http::MessageType::request );

        m_request = std::move( request );
        m_response.response->statusLine.version = request.request->requestLine.version;
        m_completionHandler = std::move( completionHandler );

        processRequest( connection, *m_request.request );
        return true;
    }

    void AbstractHttpRequestHandler::requestDone(
        const nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )
    {
        m_response.response->statusLine.statusCode = statusCode;
        m_response.response->statusLine.reasonPhrase = StatusCode::toString( statusCode );
        if( dataSource )
        {
            m_response.response->headers.emplace( "Content-Type", dataSource->mimeType() );
            const auto contentLength = dataSource->contentLength();
            if( contentLength )
                m_response.response->headers.emplace(
                    "Content-Size",
                    QByteArray::number( contentLength.get() ) );
        }

        m_completionHandler(
            std::move( m_response ),
            std::move( dataSource ) );
    }

    Response& AbstractHttpRequestHandler::response()
    {
        return *m_response.response;
    }
}
