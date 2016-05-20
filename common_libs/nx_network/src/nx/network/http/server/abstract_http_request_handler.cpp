/**********************************************************
* 20 may 2015
* a.kolesnikov
***********************************************************/

#include "abstract_http_request_handler.h"

#include <utils/common/cpp14.h>
#include <nx/network/http/server/http_stream_socket_server.h>


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
        NX_ASSERT( requestMsg.type == nx_http::MessageType::request );

        //creating response here to support pipelining
        Message responseMsg( MessageType::response );

        responseMsg.response->statusLine.version = requestMsg.request->requestLine.version;

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
            std::move(authInfo),
            std::move(std::move(*requestMsg.request)),
            m_responseMsg.response,
            sendResponseFunc);
        return true;
    }

    nx_http::Response* AbstractHttpRequestHandler::response()
    {
        return m_responseMsg.response;
    }

    void AbstractHttpRequestHandler::requestDone(
        //size_t reqID,
        const nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )
    {
        m_responseMsg.response->statusLine.statusCode = statusCode;
        m_responseMsg.response->statusLine.reasonPhrase = StatusCode::toString( statusCode );

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
