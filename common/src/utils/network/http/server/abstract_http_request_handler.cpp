/**********************************************************
* 20 may 2015
* a.kolesnikov
***********************************************************/

#include "abstract_http_request_handler.h"

#include <utils/common/cpp14.h>
#include <utils/network/http/server/http_stream_socket_server.h>


namespace nx_http
{
    AbstractHttpRequestHandler::AbstractHttpRequestHandler()
    {
    }

    AbstractHttpRequestHandler::~AbstractHttpRequestHandler()
    {
    }

    bool AbstractHttpRequestHandler::processRequest(
        const nx_http::HttpServerConnection& connection,
        nx_http::Message&& requestMsg,
        stree::ResourceContainer&& authInfo,
        std::function<
            void(
                nx_http::Message&&,
                std::unique_ptr<nx_http::AbstractMsgBodySource> )
            > completionHandler )
    {
        assert( requestMsg.type == nx_http::MessageType::request );

        //creating response here to support pipelining
        Message responseMsg( MessageType::response );

        responseMsg.response->statusLine.version = requestMsg.request->requestLine.version;

        const auto& request = *requestMsg.request;
        auto response = responseMsg.response;

        m_requestMsg = std::move( requestMsg );
        m_responseMsg = std::move( responseMsg );
        m_completionHandler = std::move( completionHandler );

        auto sendResponseFunc = [/*reqID,*/ this](
            nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )
        {
            requestDone( /*reqID,*/ statusCode, std::move( dataSource ) );
        };

        processRequest(
            connection,
            std::move( authInfo ),
            request,
            response,
            sendResponseFunc );
        return true;
    }

    void AbstractHttpRequestHandler::requestDone(
        //size_t reqID,
        const nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )
    {
        m_responseMsg.response->statusLine.statusCode = statusCode;
        m_responseMsg.response->statusLine.reasonPhrase = StatusCode::toString( statusCode );
        if( dataSource )
        {
            m_responseMsg.response->headers.emplace( "Content-Type", dataSource->mimeType() );
            const auto contentLength = dataSource->contentLength();
            if( contentLength )
                m_responseMsg.response->headers.emplace(
                    "Content-Size",
                    QByteArray::number( static_cast< qulonglong >(
                        contentLength.get() ) ) );
        }

        //this object is allowed to be removed within m_completionHandler, 
        //  so creating local data
        auto completionHandlerLocal = std::move( m_completionHandler );
        auto responseMsgLocal = std::move( m_responseMsg );
        auto dataSourceLocal = std::move( dataSource );

        completionHandlerLocal(
            std::move( responseMsgLocal ),
            std::move( dataSourceLocal ) );
    }
}
