#pragma once

#include <atomic>
#include <memory>
#include <mutex>

#include <plugins/videodecoder/stree/resourcecontainer.h>

#include "http_server_connection.h"
#include "../abstract_msg_body_source.h"

namespace nx_http {

struct NX_NETWORK_API RequestResult
{
public:
    nx_http::StatusCode::Value statusCode;
    std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource;
    ConnectionEvents connectionEvents;

    RequestResult(StatusCode::Value statusCode);
    RequestResult(
        nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource);
    RequestResult(
        nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource,
        ConnectionEvents connectionEvents);
};

typedef nx::utils::MoveOnlyFunc<void(RequestResult)> RequestProcessedHandler;

/**
 * Base class for all HTTP request processors
 * @note Class methods are not thread-safe
 */
class NX_NETWORK_API AbstractHttpRequestHandler
{
public:
    AbstractHttpRequestHandler();
    virtual ~AbstractHttpRequestHandler();

    /**
     * @param connection This object is valid only in this method. 
     *       One cannot rely on its availability after return of this method
     * @param request Message received
     * @param completionHandler Functor to be invoked to send response
     */
    bool processRequest(
        nx_http::HttpServerConnection* const connection,
        nx_http::Message&& request,
        stree::ResourceContainer&& authInfo,
        ResponseIsReadyHandler completionHandler);

protected:
    /**
     * Implement this method to handle request
     * @param response Implementation is allowed to modify response as it wishes
     * @warning This object can be removed in completionHandler 
     */
    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler ) = 0;

    nx_http::Response* response();

private:
    nx_http::Message m_responseMsg;
    ResponseIsReadyHandler m_completionHandler;

    void requestDone(RequestResult requestResult);
};

} // namespace nx_http
