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
    nx::network::http::StatusCode::Value statusCode;
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> dataSource;
    ConnectionEvents connectionEvents;

    RequestResult(StatusCode::Value statusCode);
    RequestResult(
        nx::network::http::StatusCode::Value statusCode,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> dataSource);
    RequestResult(
        nx::network::http::StatusCode::Value statusCode,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> dataSource,
        ConnectionEvents connectionEvents);
};

typedef nx::utils::MoveOnlyFunc<void(RequestResult)> RequestProcessedHandler;

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
     *       One cannot rely on its availability after return of this method
     * @param request Message received
     * @param completionHandler Functor to be invoked to send response
     */
    bool processRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::network::http::Message&& request,
        nx::utils::stree::ResourceContainer&& authInfo,
        ResponseIsReadyHandler completionHandler);

    void setRequestPathParams(std::vector<StringType> params);
    /**
     * Parameters parsed from URL path.
     * E.g., given http handler registered on path /account/%1/systems.
     * When receiving request with path /account/cartman/systems.
     * Then this method will return {cartman}.
     * Works only when using RestMessageDispatcher.
     */
    const std::vector<StringType>& requestPathParams() const;

protected:
    /**
     * Implement this method to handle request
     * @param response Implementation is allowed to modify response as it wishes
     * WARNING: This object can be removed in completionHandler
     */
    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler) = 0;

    nx::network::http::Response* response();

private:
    nx::network::http::Message m_responseMsg;
    ResponseIsReadyHandler m_completionHandler;
    std::vector<StringType> m_requestPathParams;

    void requestDone(RequestResult requestResult);
};

} // namespace nx
} // namespace network
} // namespace http
