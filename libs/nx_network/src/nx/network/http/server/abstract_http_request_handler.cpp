#include "abstract_http_request_handler.h"

#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace http {

//-------------------------------------------------------------------------------------------------
// RequestResult

RequestResult::RequestResult(StatusCode::Value statusCode):
    statusCode(statusCode)
{
}

RequestResult::RequestResult(
    nx::network::http::StatusCode::Value statusCode,
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> dataSource)
    :
    statusCode(statusCode),
    dataSource(std::move(dataSource))
{
}


RequestResult::RequestResult(
    nx::network::http::StatusCode::Value statusCode,
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> dataSource,
    ConnectionEvents connectionEvents)
    :
    statusCode(statusCode),
    dataSource(std::move(dataSource)),
    connectionEvents(std::move(connectionEvents))
{
}

//-------------------------------------------------------------------------------------------------
// AbstractHttpRequestHandler

AbstractHttpRequestHandler::~AbstractHttpRequestHandler()
{
}

bool AbstractHttpRequestHandler::processRequest(
    nx::network::http::HttpServerConnection* const connection,
    nx::network::http::Request request,
    nx::utils::stree::ResourceContainer&& authInfo,
    ResponseIsReadyHandler completionHandler)
{
    // Creating response here to support pipelining.
    Message responseMsg(MessageType::response);

    responseMsg.response->statusLine.version = request.requestLine.version;

    m_responseMsg = std::move(responseMsg);
    m_completionHandler = std::move(completionHandler);

    auto httpRequestProcessedHandler =
        [this](RequestResult requestResult)
        {
            sendResponse(std::move(requestResult));
        };

    RequestContext requestContext{
        connection,
        std::move(authInfo),
        std::move(std::move(request)),
        m_responseMsg.response,
        std::exchange(m_requestPathParams, {})};

    processRequest(
        std::move(requestContext),
        std::move(httpRequestProcessedHandler));
    return true;
}

void AbstractHttpRequestHandler::setRequestPathParams(
    RequestPathParams params)
{
    m_requestPathParams = std::move(params);
}

void AbstractHttpRequestHandler::sendResponse(RequestResult requestResult)
{
    m_responseMsg.response->statusLine.statusCode =
        requestResult.statusCode;
    m_responseMsg.response->statusLine.reasonPhrase =
        StatusCode::toString(requestResult.statusCode);

    // This object is allowed to be removed within m_completionHandler,
    //  so creating local data.
    auto completionHandlerLocal = std::move(m_completionHandler);
    auto responseMsgLocal = std::move(m_responseMsg);
    auto dataSourceLocal = std::move(requestResult.dataSource);

    completionHandlerLocal(
        std::move(responseMsgLocal),
        std::move(dataSourceLocal),
        std::move(requestResult.connectionEvents));
}

nx::network::http::Response* AbstractHttpRequestHandler::response()
{
    return m_responseMsg.response;
}

} // namespace nx
} // namespace network
} // namespace http
