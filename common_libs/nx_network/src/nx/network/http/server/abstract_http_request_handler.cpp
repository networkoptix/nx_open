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
    nx::network::http::Message&& requestMsg,
    nx::utils::stree::ResourceContainer&& authInfo,
    ResponseIsReadyHandler completionHandler)
{
    NX_ASSERT(requestMsg.type == nx::network::http::MessageType::request);

    // Creating response here to support pipelining.
    Message responseMsg(MessageType::response);

    responseMsg.response->statusLine.version = requestMsg.request->requestLine.version;

    m_responseMsg = std::move(responseMsg);
    m_completionHandler = std::move(completionHandler);

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

void AbstractHttpRequestHandler::setRequestPathParams(
    std::vector<StringType> params)
{
    m_requestPathParams = std::move(params);
}

const std::vector<StringType>& AbstractHttpRequestHandler::requestPathParams() const
{
    return m_requestPathParams;
}

nx::network::http::Response* AbstractHttpRequestHandler::response()
{
    return m_responseMsg.response;
}

void AbstractHttpRequestHandler::requestDone(RequestResult requestResult)
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

} // namespace nx
} // namespace network
} // namespace http
