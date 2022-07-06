// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/network/http/http_types.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/stree/attribute_dictionary.h>

namespace nx::network::http {

class HttpServerConnection;

/**
 * Used to install handlers on some events on HTTP connection.
 * WARNING: There is no way to remove installed event handler.
 *   Event handler implementation MUST ensure it does not crash.
 */
class ConnectionEvents
{
public:
    /**
     * Invoked just after sending the response.
     */
    nx::utils::MoveOnlyFunc<void(HttpServerConnection*)> onResponseHasBeenSent;
};

struct NX_NETWORK_API RequestResult
{
    http::StatusCode::Value statusCode;

    /**
     * The response body data source.
     */
    std::unique_ptr<http::AbstractMsgBodySource> dataSource;
    ConnectionEvents connectionEvents;

    RequestResult(StatusCode::Value statusCode);

    RequestResult(
        http::StatusCode::Value statusCode,
        std::unique_ptr<http::AbstractMsgBodySource> dataSource);

    RequestResult(
        http::StatusCode::Value statusCode,
        std::unique_ptr<http::AbstractMsgBodySource> dataSource,
        ConnectionEvents connectionEvents);
};

using RequestProcessedHandler = nx::utils::MoveOnlyFunc<void(RequestResult)>;

struct RequestContext
{
    http::HttpServerConnection* connection = nullptr;

    /**
     * The endpoint of the request originator. Note that this is different from the HTTP connection
     * source endpoint if HTTP proxy is inplace. In this case, Forwarded and X-Forwarded-For headers
     * are taken into account.
     */
    SocketAddress clientEndpoint;

    /**
     * Attributes added by the authenticator when authenticating the request.
     */
    nx::utils::stree::AttributeDictionary authInfo;
    http::Request request;

    /**
     * Initialized only if the HTTP handler is registered using MessageBodyDeliveryType::stream.
     */
    std::unique_ptr<AbstractMsgBodySourceWithCache> body;
    http::Response* response = nullptr;

    /**
     * Parameters, taken from request path.
     * E.g., if handler was registered with path /object_type/{objectId}/sub_object_type/{subObjectId},
     * and request /object_type/id1/sub_object_type/id2 was received.
     * Then this would be (("objectId", "id1"), ("subObjectId", "id2")).
     */
    RequestPathParams requestPathParams;

    RequestContext() = default;

    RequestContext(
        http::HttpServerConnection* connection,
        const SocketAddress& clientEndpoint,
        nx::utils::stree::AttributeDictionary authInfo,
        http::Request request,
        std::unique_ptr<AbstractMsgBodySourceWithCache> body = nullptr)
        :
        connection(connection),
        clientEndpoint(clientEndpoint),
        authInfo(std::move(authInfo)),
        request(std::move(request)),
        body(std::move(body))
    {
    }
};

enum class MessageBodyDeliveryType
{
    /**
     * The message body will be delivered to the AbstractHttpRequestHandler descendant in
     * http::RequestContext.request.messageBody.
     */
    buffer,

    /**
     * The message body will be delivered to the AbstractHttpRequestHandler descendant in
     * http::RequestContext.body.
     */
    stream,
};

} // namespace nx::network::http
