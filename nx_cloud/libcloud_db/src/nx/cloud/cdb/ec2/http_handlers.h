#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>

namespace nx {
namespace cdb {

template<typename Func>
class CustomHttpHandler:
    public nx_http::AbstractHttpRequestHandler
{
public:
    CustomHttpHandler(Func func):
        m_func(std::move(func))
    {
    }

protected:
    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler) override
    {
        m_func(
            connection,
            std::move(authInfo),
            std::move(request),
            response,
            std::move(completionHandler));
    }

private:
    Func m_func;
};

template<typename Func>
void registerCustomHttpHandler(
    nx_http::MessageDispatcher* const httpMessageDispatcher,
    const QString& requestPath,
    Func func)
{
    typedef typename CustomHttpHandler<Func> RequestHandlerType;

    httpMessageDispatcher->registerRequestProcessor<RequestHandlerType>(
        requestPath,
        [func]() -> std::unique_ptr<RequestHandlerType>
        {
            return std::make_unique<RequestHandlerType>(func);
        });
}

} // namespace cdb
} // namespace nx
