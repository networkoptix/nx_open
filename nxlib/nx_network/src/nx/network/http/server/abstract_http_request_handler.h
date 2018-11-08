#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <nx/utils/stree/resourcecontainer.h>

#include "http_server_connection.h"
#include "../abstract_msg_body_source.h"

namespace nx {
namespace network {
namespace http {

struct NX_NETWORK_API RequestResult
{
    http::StatusCode::Value statusCode;
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
    nx::utils::stree::ResourceContainer authInfo;
    http::Request request;
    http::Response* response = nullptr;
    /**
     * Parameters, taken from request path. 
     * E.g., if handler was registered with path /object_type/{objectId}/sub_object_type/{subObjectId},
     * and request /object_type/id1/sub_object_type/id2 was received.
     * Then this would be (("objectId", "id1"), ("subObjectId", "id2")).
     */
    RequestPathParams requestPathParams;
};

/**
 * Base class for all HTTP request processors
 * NOTE: Class methods are not thread-safe
 */
class NX_NETWORK_API AbstractHttpRequestHandler
{
public:
    virtual ~AbstractHttpRequestHandler();

    /**
     * @param connection This object is valid only in this method.
     *     One cannot rely on its availability after return of this method.
     * @param completionHandler Functor to be invoked to send response.
     */
    bool processRequest(
        http::HttpServerConnection* const connection,
        http::Request request,
        nx::utils::stree::ResourceContainer&& authInfo,
        ResponseIsReadyHandler completionHandler);

    void setRequestPathParams(RequestPathParams params);

protected:
    /**
     * Implement this method to handle request
     * @param response Implementation is allowed to modify response as it wishes
     * WARNING: This object can be removed in completionHandler
     */
    virtual void processRequest(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler) = 0;

    virtual void sendResponse(RequestResult requestResult);

    http::Response* response();

private:
    http::Message m_responseMsg;
    ResponseIsReadyHandler m_completionHandler;
    RequestPathParams m_requestPathParams;
};

} // namespace nx
} // namespace network
} // namespace http
