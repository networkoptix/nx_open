// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_http_request_handler.h"

#include <nx/utils/std/cpp14.h>

#include "http_server_connection.h"
#include "http_stream_socket_server.h"

namespace nx::network::http {

//-------------------------------------------------------------------------------------------------
// RequestHandlerWithContext

void RequestHandlerWithContext::serve(
    RequestContext requestContext,
    RequestProcessedHandler completionHandler)
{
    m_responseMsg = Message(MessageType::response);
    m_responseMsg.response->statusLine.version = requestContext.request.requestLine.version;

    m_requestBodySource = std::move(requestContext.body);

    m_completionHandler = std::move(completionHandler);

    m_requestContext = std::move(requestContext);
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

void RequestHandlerWithContext::setRequestBodyDeliveryType(
    MessageBodyDeliveryType value)
{
    m_messageBodyDeliveryType = value;
}

void RequestHandlerWithContext::setRequestPathParams(
    RequestPathParams params)
{
    m_requestPathParams = std::move(params);
}

nx::network::http::Response* RequestHandlerWithContext::response()
{
    return m_responseMsg.response;
}

void RequestHandlerWithContext::readMessageBodyAsync()
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

bool RequestHandlerWithContext::processMessageBodyBuffer(
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

void RequestHandlerWithContext::propagateRequest()
{
    processRequest(
        std::exchange(m_requestContext, {}),
        [this](auto&&... args) { sendResponse(std::forward<decltype(args)>(args)...); });
}

void RequestHandlerWithContext::sendResponse(RequestResult result)
{
    if (m_responseMsg.response)
        result.headers.merge(std::move(m_responseMsg.response->headers));

    m_completionHandler(std::move(result));
}

} // namespace nx::network::http
