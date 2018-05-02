#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>

namespace nx {
namespace cdb {

template<typename Func>
class CustomHttpHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    CustomHttpHandler(Func func):
        m_func(std::move(func))
    {
    }

protected:
    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler) override
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
    nx::network::http::MessageDispatcher* const httpMessageDispatcher,
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
