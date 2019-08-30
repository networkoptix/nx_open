#pragma once

#include <nx/network/http/server/abstract_fusion_request_handler.h>
#include <nx/network/socket_common.h>

namespace nx::network::http::server::rest {

template<const char* name>
class RestArgFetcher
{
public:
    using type = std::string;

    type operator()(
        const network::http::RequestContext& requestContext) const
    {
        return requestContext.requestPathParams.getByName(name);
    }
};

class ClientEndpointFetcher
{
public:
    using type = network::SocketAddress;

    type operator()(
        const network::http::RequestContext& requestContext) const
    {
        return requestContext.connection->clientEndpoint();
    }
};

class AuthInfoFetcher
{
public:
    using type = nx::utils::stree::ResourceContainer;

    type operator()(
        const network::http::RequestContext& requestContext) const
    {
        return requestContext.authInfo;
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

/**
 * RequestHandler class that can be used to standardize http request error processing.
 * Usage example:
 *
 * struct FooResult { bool ok; };
 * struct FooRequest { ... };
 * struct FooResponse { std::string id; };
 *
 * class FooManager
 * {
 * public:
 *     void putFoo(
 *         FooRequest request,
 *         nx::utils::MoveOnlyFunc<void(FooResult, FooResponse)> handler);
 *
 *     // NOTE: void input type. id requires parsing of http request path "/foo/{id}"
 *     void getFoo(
 *           std::string id,
 *           nx::utils::MoveOnlyFunc<void(FooResult, FooResponse)> handler);
 *
 *    // NOTE: requires getting the client's IP address from the http request
 *    void deleteFooAndLogClientEndoint(
 *        std::string id,
 *        network::SocketAddress clientEndpoint,
 *        nx::utils::MoveOnlyFunc<void(FooResult)> handler);
 * };
 *
 * template<
 *   typename Result,
 *   typename Input,
 *   typename Output,
 *   typename... RestArgFetchers>
 * class MyRequestHandler: public BaseRequestHandler
 * {
 * public:
 *     template<typename... OutputData>
 *     void processResponseInternal(FooResult result, OutputData... output)
 *     {
 *         if (result.ok)
 *         {
 *             this->RequestCompleted(network::http::FusionRequestResult(), std::move(output)...);
 *         }
 *         else
 *         {
 *             network::http::FusionRequestResult error = fooResultToFusionRequestError(result);
 *             this->requestCompleted(std::move(error), std:move(output)...);
 *         }
 *     }
 * };
 *
 * class MyHttpServer
 * {
 * public:
 *     void registerFooManagerApi();
 *
 * private:
 *     network::http::server::rest::MessageDispatcher m_dispatcher;
 *     FooManager m_fooManager;
 * }
 *
 * void MyHttpServer::registerFooManagerApi()
 * {
 *     using namespace std::placeholders;
 *
 *     static constexpr char kIdParam[] = "id";
 *
 *     using PutFooHandler = MyRequestHandler<FooRequest, FooResponse>;
 *     m_dispatcher.registerRequestProcessor<PutFooHandler>(
 *     "/foo",
 *     [this]()
 *     {
 *          return std::make_unique<PutFooHandler>(
 *              std::bind(&FooManager::putFoo, &m_fooManager, _1, _2));
 *     },
 *     network::http::Method::put);
 *
 *     using GetFooHandler =
 *         MyRequestHandler<void, FooResult, RestArgFetcher<kIdParam>>;
 *     m_dispatcher.registerRequestProcessor<GetFooHandler>(
 *     "/foo/{id}",
 *     [this]()
 *     {
 *          return std::make_unique<GetFooHandler>(
 *              std::bind(&FooManager::getFoo, &m_fooManager, _1, _2));
 *     },
 *     network::http::Method::get);
 *
 *     // NOTE: this request has no fusion input and no fusion output.
 *     // NOTE: RestArgFetcher<kIdParam> and ClientEndpointFetcher must occur in the same order
 *     // as the parameters they correspond to.

 *     using DeleteFooHandler =
 *         MyRequestHandler<void, void, RestArgFetcher<kIdParam>, ClientEndpointFetcher>;
 *     m_dispatcher.registerRequestProcessor<DeleteFooHandler>(
 *     "foo/{id}",
 *     [this]()
 *     {
 *         return std::make_unique<DeleteFooHandler>(
 *             std::bind(&FooManager::deleteFooAndLogClientEndpoint, &m_fooManager, _1, _2, _3));
 *     },
 *     network::http::Method::delete_);
 * }
 */

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
