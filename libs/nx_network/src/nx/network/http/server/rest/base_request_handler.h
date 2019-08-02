#pragma once

#include <nx/network/http/server/abstract_fusion_request_handler.h>
#include <nx/network/socket_common.h>

namespace nx::network::http::server::rest {

template<const char* name>
class RestArgFetcher
{
public:
    using type = std::string;

    std::string operator()(
        const network::http::RequestContext& requestContext) const
    {
        return requestContext.requestPathParams.getByName(name);
    }
};

class ClientEndpointFetcher
{
public:
    using type = network::SocketAddress;

    network::SocketAddress operator()(
        const network::http::RequestContext& requestContext) const
    {
        return requestContext.connection->clientEndpoint();
    }
};

template<typename Result, typename Data>
struct CompletionHandlerDeclarator
{
    using Func = std::function<void(Result, Data)>;
};

template<typename Result>
struct CompletionHandlerDeclarator<Result, void>
{
    using Func = std::function<void(Result)>;
};

//-------------------------------------------------------------------------------------------------

template<
    typename Derived,
    typename Result,
    typename Input,
    typename Output,
    typename... RestArgFetchers>
class BaseRequestHandler:
    public network::http::AbstractFusionRequestHandler<Input, Output>
{
public:
    using CompletionHandler = typename CompletionHandlerDeclarator<Result, Output>::Func;

    using BusinessFunc = std::function<void(
        typename RestArgFetchers::type...,
        Input input,
        CompletionHandler)>;

    BaseRequestHandler(BusinessFunc businessFunc):
        m_businessFunc(std::move(businessFunc))
    {
    }

    virtual void processRequest(
        network::http::RequestContext requestContext,
        Input request) override;

private:
    BusinessFunc m_businessFunc;
};

template<
    typename Derived,
    typename Result,
    typename Input,
    typename Output,
    typename... RestArgFetchers>
void BaseRequestHandler<Derived, Result, Input, Output, RestArgFetchers...>::processRequest(
    network::http::RequestContext requestContext,
    Input input)
{
    m_businessFunc(
        RestArgFetchers()(requestContext)...,
        std::move(input),
        [this](auto&& ... args)
        {
            static_cast<Derived*>(this)->processResponseInternal(
                std::forward<decltype(args)>(args)...);
        });
}

//-------------------------------------------------------------------------------------------------
// void Input specialization

template<typename Derived, typename Result, typename Output, typename... RestArgFetchers>
class BaseRequestHandler<Derived, Result, void, Output, RestArgFetchers...>:
    public network::http::AbstractFusionRequestHandler<void, Output>
{
public:
    using CompletionHandler = typename CompletionHandlerDeclarator<Result, Output>::Func;

    using BusinessFunc = std::function<void(
        typename RestArgFetchers::type...,
        CompletionHandler)>;

    BaseRequestHandler(BusinessFunc businessFunc):
        m_businessFunc(std::move(businessFunc))
    {
    }

    virtual void processRequest(
        network::http::RequestContext requestContext) override;

private:
    BusinessFunc m_businessFunc;
};

template<typename Derived, typename Result, typename Output, typename... RestArgFetchers>
void BaseRequestHandler<Derived, Result, void, Output, RestArgFetchers...>::processRequest(
    network::http::RequestContext requestContext)
{
    m_businessFunc(
        RestArgFetchers()(requestContext)...,
        [this](auto&& ... args)
        {
            static_cast<Derived*>(this)->processResponseInternal(
                std::forward<decltype(args)>(args)...);
        });
}

} // namespace nx::network::http::server::rest
