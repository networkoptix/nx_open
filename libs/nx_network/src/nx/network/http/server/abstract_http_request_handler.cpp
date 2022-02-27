// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_http_request_handler.h"

#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/utils/std/cpp14.h>

namespace nx::network::http {

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

void AbstractHttpRequestHandler::handleRequest(
    RequestContext requestContext,
    ResponseIsReadyHandler completionHandler)
{
    m_responseMsg = Message(MessageType::response);
    m_responseMsg.response->statusLine.version = requestContext.request.requestLine.version;

    m_requestBodySource = std::move(requestContext.body);

    m_completionHandler = std::move(completionHandler);

    m_requestContext = std::move(requestContext);
    m_requestContext.response = m_responseMsg.response;
    m_requestContext.requestPathParams = std::exchange(m_requestPathParams, {});

    if (!m_requestBodySource ||
        m_messageBodyDeliveryType == MessageBodyDeliveryType::stream)
    {
        m_requestContext.body = std::exchange(m_requestBodySource, nullptr);
        return propagateRequest();
    }

    while (auto data = m_requestBodySource->peek())
    {
        auto& [resultCode, buffer] = *data;

        const bool reachedMessageBodyEnd =
            processMessageBodyBuffer(resultCode, std::move(buffer));
        if (reachedMessageBodyEnd)
            return;
    }

    readMessageBodyAsync();
}

void AbstractHttpRequestHandler::setRequestBodyDeliveryType(
    MessageBodyDeliveryType value)
{
    m_messageBodyDeliveryType = value;
}

void AbstractHttpRequestHandler::setRequestPathParams(
    RequestPathParams params)
{
    m_requestPathParams = std::move(params);
}

nx::network::http::Response* AbstractHttpRequestHandler::response()
{
    return m_responseMsg.response;
}

void AbstractHttpRequestHandler::readMessageBodyAsync()
{
    m_requestBodySource->readAsync(
        [this](auto&&... args)
        {
            const bool reachedMessageBodyEnd =
                processMessageBodyBuffer(std::forward<decltype(args)>(args)...);
            if (reachedMessageBodyEnd)
                return;

            readMessageBodyAsync();
        });
}

bool AbstractHttpRequestHandler::processMessageBodyBuffer(
    SystemError::ErrorCode resultCode, nx::Buffer buffer)
{
    if (resultCode != SystemError::noError)
    {
        NX_VERBOSE(this, "Request %1. Error reading request body: %2",
            m_requestContext.request.requestLine, SystemError::toString(resultCode));
        sendResponse(RequestResult(StatusCode::badRequest));
        return true;
    }

    if (buffer.empty())
    {
        // The complete message body has been read.
        NX_VERBOSE(this, "Request %1. The complete request body (%2 bytes) has been received",
            m_requestContext.request.requestLine, m_requestContext.request.messageBody.size());
        propagateRequest();
        return true;
    }

    m_requestContext.request.messageBody += std::move(buffer);

    return false;
}

void AbstractHttpRequestHandler::propagateRequest()
{
    processRequest(
        std::exchange(m_requestContext, {}),
        [this](auto&&... args) { sendResponse(std::forward<decltype(args)>(args)...); });
}

void AbstractHttpRequestHandler::sendResponse(RequestResult requestResult)
{
    m_responseMsg.response->statusLine.statusCode =
        requestResult.statusCode;
    m_responseMsg.response->statusLine.reasonPhrase =
        StatusCode::toString(requestResult.statusCode);

    m_completionHandler(
        std::exchange(m_responseMsg, {}),
        std::exchange(requestResult.dataSource, nullptr),
        std::move(requestResult.connectionEvents));
}

} // namespace nx::network::http
