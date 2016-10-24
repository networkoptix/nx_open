#include "abstract_http_request_handler.h"

#include <nx/utils/std/cpp14.h>
#include <nx/network/http/server/http_stream_socket_server.h>

namespace nx_http {

//-------------------------------------------------------------------------------------------------
// RequestResult

RequestResult::RequestResult(StatusCode::Value _statusCode):
    statusCode(_statusCode)
{
}

RequestResult::RequestResult(
    nx_http::StatusCode::Value _statusCode,
    std::unique_ptr<nx_http::AbstractMsgBodySource> _dataSource)
    :
    statusCode(_statusCode),
    dataSource(std::move(_dataSource))
{
}


RequestResult::RequestResult(
    nx_http::StatusCode::Value _statusCode,
    std::unique_ptr<nx_http::AbstractMsgBodySource> _dataSource,
    ConnectionEvents _connectionEvents)
    :
    statusCode(_statusCode),
    dataSource(std::move(_dataSource)),
    connectionEvents(std::move(_connectionEvents))
{
}

//-------------------------------------------------------------------------------------------------
// AbstractHttpRequestHandler

AbstractHttpRequestHandler::AbstractHttpRequestHandler()
{
}

AbstractHttpRequestHandler::~AbstractHttpRequestHandler()
{
}

bool AbstractHttpRequestHandler::processRequest(
    nx_http::HttpServerConnection* const connection,
    nx_http::Message&& requestMsg,
    stree::ResourceContainer&& authInfo,
    ResponseIsReadyHandler completionHandler )
{
    NX_ASSERT( requestMsg.type == nx_http::MessageType::request );

    //creating response here to support pipelining
    Message responseMsg( MessageType::response );

    responseMsg.response->statusLine.version = requestMsg.request->requestLine.version;

    m_responseMsg = std::move( responseMsg );
    m_completionHandler = std::move( completionHandler );

    auto httpRequestProcessedHandler =
        [this](RequestResult requestResult)
        {
            requestDone(std::move(requestResult));
        };

    processRequest(
        connection,
        std::move(authInfo),
        std::move(std::move(*requestMsg.request)),
        m_responseMsg.response,
        std::move(httpRequestProcessedHandler));
    return true;
}

nx_http::Response* AbstractHttpRequestHandler::response()
{
    return m_responseMsg.response;
}

void AbstractHttpRequestHandler::requestDone(RequestResult requestResult)
{
    m_responseMsg.response->statusLine.statusCode =
        requestResult.statusCode;
    m_responseMsg.response->statusLine.reasonPhrase =
        StatusCode::toString(requestResult.statusCode);

    //this object is allowed to be removed within m_completionHandler, 
    //  so creating local data
    auto completionHandlerLocal = std::move(m_completionHandler);
    auto responseMsgLocal = std::move(m_responseMsg);
    auto dataSourceLocal = std::move(requestResult.dataSource);

    completionHandlerLocal(
        std::move(responseMsgLocal),
        std::move(dataSourceLocal),
        std::move(requestResult.connectionEvents));
}

} // namespace nx_http
