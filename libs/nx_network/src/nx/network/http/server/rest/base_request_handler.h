// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/server/abstract_api_request_handler.h>
#include <nx/network/socket_common.h>
#include <nx/network/utils.h>
#include <nx/utils/url_query.h>

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
        return requestContext.clientEndpoint;
    }
};

class AttrsFetcher
{
public:
    using type = decltype(network::http::RequestContext::attrs);

    const type& operator()(
        const network::http::RequestContext& requestContext) const
    {
        return requestContext.attrs;
    }
};

class UrlQueryFetcher
{
public:
    using type = nx::utils::UrlQuery;

    type operator()(
        const network::http::RequestContext& requestContext) const
    {
        return nx::utils::UrlQuery(requestContext.request.requestLine.url.query());
    }
};

template<const char* Name>
class HeaderFetcher
{
public:
    using type = std::optional<std::string>;

    static constexpr std::string_view kName{Name};

    type operator()(const network::http::RequestContext& requestContext) const
    {
        if (auto it = requestContext.request.headers.find(kName);
            it != requestContext.request.headers.end())
        {
            return it->second;
        }

        return std::nullopt;
    }
};

class HostHeaderFetcher
{
public:
    using type = std::optional<std::string>;

    type operator()(const network::http::RequestContext& requestContext) const
    {
        return nx::network::util::getHostName(requestContext);
    }
};

/**
 * Detects if the connection request was received over can be considered TLS-secure.
 * I.e., it is either
 * - a TLS connection,
 * - or a LAN connection and "X-Forwarded-Proto: https" header can be found in the request.
 */
class HttpsRequestDetector
{
public:
    using type = bool;

    type operator()(
        const network::http::RequestContext& requestContext) const
    {
        return (*this)(requestContext.connectionAttrs, requestContext.request);
    }

    type operator()(
        const nx::network::http::ConnectionAttrs& connectionAttrs,
        const nx::network::http::Request& request) const
    {
        const auto xForwardedProtoValue = getHeaderValue(
            request.headers, header::XForwardedProto::NAME);
        const bool isSslConnection = connectionAttrs.isSsl
            || (connectionAttrs.sourceAddr.address.isLocalNetwork()
                && xForwardedProtoValue == header::XForwardedProto::kHttps);
        return isSslConnection;
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

inline static void convert(const StatusCode::Value& status, ApiRequestResult* httpResult)
{
    *httpResult = ApiRequestResult(status);
}

//-------------------------------------------------------------------------------------------------

/**
 * RequestHandler class that can be used to standardize http request error processing.
 * Usage example:
 *
 * struct FooResult { bool ok; };
 * struct FooRequest { ... };
 * struct FooResponse { std::string id; };
 *
 * // The application logic class that we want to invoke via HTTP REST API.
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
 *    void deleteFooAndLogClientEndpoint(
 *        std::string id,
 *        network::SocketAddress clientEndpoint,
 *        nx::utils::MoveOnlyFunc<void(FooResult)> handler);
 * };
 *
 * // The application has to provide the following function:
 * void convert(const FooResult& result, ApiRequestResult* httpResult);
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
 *     using PutFooHandler = http::server::rest::RequestHandler<
 *         FooResult, FooRequest, FooResponse>;
 *     m_dispatcher.registerRequestProcessor(
 *         "/foo",
 *         [this]()
 *         {
 *             return std::make_unique<PutFooHandler>(
 *                 std::bind(&FooManager::putFoo, &m_fooManager, _1, _2));
 *         },
 *         network::http::Method::put);
 *
 *     using GetFooHandler = http::server::rest::RequestHandler<
 *         FooResult, void, FooResponse, RestArgFetcher<kIdParam>>;
 *     m_dispatcher.registerRequestProcessor(
 *         "/foo/{id}",
 *         [this]()
 *         {
 *             return std::make_unique<GetFooHandler>(
 *                 std::bind(&FooManager::getFoo, &m_fooManager, _1, _2));
 *         },
 *         network::http::Method::get);
 *
 *     // NOTE: this request has no deserializable input and no serializable output.
 *     // NOTE: RestArgFetcher<kIdParam> and ClientEndpointFetcher must occur in the same order
 *     // as the parameters they correspond to.

 *     using DeleteFooHandler = http::server::rest::RequestHandler<
 *         FooResult, void, void, RestArgFetcher<kIdParam>, ClientEndpointFetcher>;
 *     m_dispatcher.registerRequestProcessor(
 *         "foo/{id}",
 *         [this]()
 *         {
 *             return std::make_unique<DeleteFooHandler>(
 *                 std::bind(&FooManager::deleteFooAndLogClientEndpoint, &m_fooManager, _1, _2, _3));
 *         },
 *         network::http::Method::delete_);
 * }
 */
template<
    typename Result,
    typename Input,
    typename Output,
    typename... RestArgFetchers>
class RequestHandler:
    public network::http::AbstractApiRequestHandler<Input>
{
public:
    using CompletionHandler = typename CompletionHandlerDeclarator<Result, Output>::Func;

    using BusinessFunc = std::function<void(
        typename RestArgFetchers::type...,
        Input input,
        CompletionHandler)>;

    RequestHandler(BusinessFunc businessFunc):
        m_businessFunc(std::move(businessFunc))
    {
    }

    virtual void processRequest(
        network::http::RequestContext requestContext,
        Input request) override;

private:
    BusinessFunc m_businessFunc;

    template<typename... OutputArgs>
    void processResponseInternal(const Result& result, OutputArgs&&... output);
};

template<
    typename Result,
    typename Input,
    typename Output,
    typename... RestArgFetchers>
void RequestHandler<Result, Input, Output, RestArgFetchers...>::processRequest(
    network::http::RequestContext requestContext,
    Input input)
{
    m_businessFunc(
        RestArgFetchers()(requestContext)...,
        std::move(input),
        [this](auto&&... args)
        {
            this->processResponseInternal(std::forward<decltype(args)>(args)...);
        });
}

template<
    typename Result,
    typename Input,
    typename Output,
    typename... RestArgFetchers>
template<typename... OutputArgs>
void RequestHandler<Result, Input, Output, RestArgFetchers...>::processResponseInternal(
    const Result& result, OutputArgs&&... output)
{
    network::http::ApiRequestResult httpResult;
    convert(result, &httpResult);
    this->requestCompleted(httpResult, std::forward<OutputArgs>(output)...);
}

//-------------------------------------------------------------------------------------------------
// void Input specialization

template<typename Result, typename Output, typename... RestArgFetchers>
class RequestHandler<Result, void, Output, RestArgFetchers...>:
    public network::http::AbstractApiRequestHandler<>
{
public:
    using CompletionHandler = typename CompletionHandlerDeclarator<Result, Output>::Func;

    using BusinessFunc = std::function<void(
        typename RestArgFetchers::type...,
        CompletionHandler)>;

    RequestHandler(BusinessFunc businessFunc):
        m_businessFunc(std::move(businessFunc))
    {
    }

    virtual void processRequest(
        network::http::RequestContext requestContext) override;

private:
    BusinessFunc m_businessFunc;

    template<typename... OutputArgs>
    void processResponseInternal(const Result& result, OutputArgs&&... output);
};

template<typename Result, typename Output, typename... RestArgFetchers>
void RequestHandler<Result, void, Output, RestArgFetchers...>::processRequest(
    network::http::RequestContext requestContext)
{
    m_businessFunc(
        RestArgFetchers()(requestContext)...,
        [this](auto&&... args)
        {
            this->processResponseInternal(std::forward<decltype(args)>(args)...);
        });
}

template<typename Result, typename Output, typename... RestArgFetchers>
template<typename... OutputArgs>
void RequestHandler<Result, void, Output, RestArgFetchers...>::processResponseInternal(
    const Result& result, OutputArgs&&... output)
{
    network::http::ApiRequestResult httpResult;
    convert(result, &httpResult);
    this->requestCompleted(httpResult, std::forward<OutputArgs>(output)...);
}

} // namespace nx::network::http::server::rest
