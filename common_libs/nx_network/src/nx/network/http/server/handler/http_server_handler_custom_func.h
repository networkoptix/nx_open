#pragma once

#include <memory>

#include <nx/utils/move_only_func.h>

#include "../abstract_http_request_handler.h"

namespace nx_http {
namespace server {
namespace handler {

class CustomFunc:
    public AbstractHttpRequestHandler
{
public:
    using ProcessHttpRequestFunc = nx::utils::MoveOnlyFunc<void(
        nx_http::HttpServerConnection* const /*connection*/,
        stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler /*completionHandler*/)>;

    CustomFunc(ProcessHttpRequestFunc func);

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler) override;

private:
    ProcessHttpRequestFunc m_func;
};

} // namespace handler
} // namespace server
} // namespace nx_http
