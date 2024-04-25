// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>

#include <nx/network/connection_server/message_dispatcher.h>
#include <nx/utils/counter.h>
#include <nx/utils/data_structures/partitioned_concurrent_hash_map.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>

#include "abstract_http_request_handler.h"
#include "handler/http_server_handler_custom.h"
#include "handler/http_server_handler_redirect.h"
#include "http_server_connection.h"
#include "http_server_exact_path_matcher.h"
#include "http_statistics.h"

namespace nx::network::http {

static constexpr std::string_view kAnyMethod = "";
static constexpr char kAnyPath[] = "";

template<typename Value>
const std::pair<const std::string, Value>* findByMaxPrefix(
    const std::map<std::string, Value>& map, const std::string& key)
{
    auto it = map.upper_bound(key);
    if (it == map.begin())
        return nullptr;

    --it;
    if (!nx::utils::startsWith(key, it->first))
        return nullptr;

    return &(*it);
}

//-------------------------------------------------------------------------------------------------

/**
 * Dispatches HTTP requests to a proper message processor.
 * Provides a pure virtual AbstractMessageDispatcher::getHandler method that must be implemented by
 * a descendant.
 */
class NX_NETWORK_API AbstractMessageDispatcher:
    public AbstractRequestHandler
{
public:
    using HandlerFactoryFunc = std::function<std::unique_ptr<RequestHandlerWithContext>()>;

    AbstractMessageDispatcher();
    virtual ~AbstractMessageDispatcher();

    virtual void serve(
        RequestContext requestContext,
        nx::utils::MoveOnlyFunc<void(RequestResult)> completionHandler) override;

    /**
     * @return false if some handler is already registered for path.
     */
    virtual bool registerRequestProcessor(
        const std::string_view& path,
        HandlerFactoryFunc factoryFunc,
        const Method& method = kAnyMethod) = 0;

    template<typename Func>
    bool registerRequestProcessorFunc(
        const Method& method,
        const std::string& path,
        Func func,
        MessageBodyDeliveryType requestBodyDeliveryType = MessageBodyDeliveryType::buffer)
    {
        using RequestHandlerType =
            nx::network::http::server::handler::CustomRequestHandler<Func>;

        return registerRequestProcessor(
            path,
            [func = std::move(func), requestBodyDeliveryType]()
            {
                auto handler = std::make_unique<RequestHandlerType>(std::move(func));
                handler->setRequestBodyDeliveryType(requestBodyDeliveryType);
                return handler;
            },
            method);
    }

    bool registerRedirect(
        const std::string& sourcePath,
        const std::string& destinationPath,
        const Method& method = kAnyMethod);

    /**
     * Pass message to corresponding processor.
     *
     *  @param message This object is not moved in case of failure to find processor.
     *  @return true if request processing passed to corresponding processor and async processing
     *      has been started, false otherwise.
     */
    template<class CompletionFuncRefType>
    bool dispatchRequest(
        RequestContext requestContext,
        CompletionFuncRefType completionFunc) const
    {
        applyModRewrite(&requestContext.request.requestLine.url);

        auto handlerContext = getHandler(
            requestContext.request.requestLine.method,
            requestContext.request.requestLine.url.path().toStdString());
        if (!handlerContext)
        {
            recordDispatchFailure();
            return false;
        }

        auto statisticsKey = requestContext.request.requestLine.method.toString() + " " +
            handlerContext->pathTemplate;

        // NOTE: Cannot capture scoped increment in lambda since the capture variable
        // destruction order is unspecified.
        m_runningRequestCounter->increment();

        const auto handlerPtr = handlerContext->handler.get();
        auto requestProcessStartTime = std::chrono::steady_clock::now();

        const int seq = ++m_requestSeq;
        m_activeRequests.emplace(seq, requestContext.request.requestLine.toString());

        handlerPtr->serve(
            std::move(requestContext),
            [this, handler = std::move(handlerContext->handler), seq,
                completionFunc = std::move(completionFunc), counter = m_runningRequestCounter,
                requestProcessStartTime, statisticsKey = std::move(statisticsKey)](
                    RequestResult result) mutable
            {
                using namespace std::chrono;
                recordStatistics(
                    result,
                    statisticsKey,
                    duration_cast<microseconds>(steady_clock::now() - requestProcessStartTime));

                m_activeRequests.erase(seq);

                completionFunc(std::move(result));

                // Creating copy of counter since this lambda is destroyed by handler.reset().
                auto localCounter = counter;
                handler.reset();
                localCounter->decrement();
            });

        return true;
    }

    bool waitUntilAllRequestsCompleted(
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * Get the frequency per minute with which each HTTP Status code is occuring.
     */
    std::map<int /*http status*/, int /*count*/> statusCodesReported() const;

    // NOTE: RequestPathStatistics values that have requestsServedPerMinute == 0 are not
    // included to avoid empty values in statistics reports.
    std::map<std::string, server::RequestStatistics> requestPathStatistics() const;

    static constexpr auto kDefaultLinger = std::chrono::seconds(17);

    /**
     * Linger specifies whether the object waits for all requests to be completed during
     * the destruction.
     * By default, kDefaultLinger is used.
     */
    void setLinger(std::optional<std::chrono::milliseconds> timeout);

protected:
    struct HandlerContext
    {
        std::unique_ptr<RequestHandlerWithContext> handler;
        std::string pathTemplate;
    };

protected:
    virtual void applyModRewrite(nx::utils::Url* url) const = 0;

    /**
     * @return HTTP message handler functor that corresponds to the given HTTP method and path.
     */
    virtual std::optional<HandlerContext> getHandler(
        const Method& method,
        const std::string& path) const = 0;

private:
    void recordDispatchFailure() const;
    void recordStatistics(
        const RequestResult& result,
        const std::string& requestPathTemplate,
        std::chrono::microseconds processingTime) const;

private:
    /**
     * Using shared_ptr here since some dispatcher usages may destroy the dispatcher
     * before all requests have completed. This was fine before introduction of this counter.
     */
    std::shared_ptr<nx::utils::Counter> m_runningRequestCounter;

    mutable nx::Mutex m_mutex;
    mutable std::map <StatusCode::Value, nx::utils::math::SumPerMinute<int>> m_statusCodesPerMinute;

    mutable nx::utils::PartitionedConcurrentHashMap<
        std::string, server::RequestStatisticsCalculator
    > m_requestPathStatsCalculators;

    mutable nx::utils::PartitionedConcurrentHashMap<int /*sequence*/, std::string> m_activeRequests;
    mutable std::atomic<int> m_requestSeq{0};
    std::optional<std::chrono::milliseconds> m_linger = kDefaultLinger;
};

//-------------------------------------------------------------------------------------------------

// TODO: #akolesnikov Make it a concept.
template<typename Mapped>
class AbstractPathMatcher
{
public:
    virtual bool add(const std::string_view& path, Mapped mapped) = 0;

    virtual std::optional<std::reference_wrapper<const Mapped>> match(
        const std::string_view& path,
        RequestPathParams* /*pathParams*/,
        std::string* /*pathTemplate*/) const = 0;
};

/**
 * Implements AbstractMessageDispatcher.
 * Uses PathMatcherType to choose a handler for a given method/path.
 * PathMatcherType must satisfy AbstractPathMatcher. Inheriting it is not required.
 */
template<template<typename> class PathMatcherType>
class BasicMessageDispatcher:
    public AbstractMessageDispatcher
{
public:
    virtual ~BasicMessageDispatcher() = default;

    virtual bool registerRequestProcessor(
        const std::string_view& path,
        HandlerFactoryFunc factoryFunc,
        const Method& method = kAnyMethod) override
    {
        NX_ASSERT(factoryFunc);
        auto handlerFactory = HandlerFactory{std::move(factoryFunc)};

        PathMatchContext& pathMatchContext = m_factories[method];
        if (path == kAnyPath)
        {
            if (pathMatchContext.defaultHandlerFactory)
                return false;
            pathMatchContext.defaultHandlerFactory = std::move(handlerFactory);
            return true;
        }

        return pathMatchContext.pathToFactory.add(path, std::move(handlerFactory));
    }

    template<typename RequestHandlerType>
    bool registerRequestProcessor(
        const std::string_view& path = kAnyPath,
        const Method& method = kAnyMethod)
    {
        return registerRequestProcessor(
            path,
            []() { return std::make_unique<RequestHandlerType>(); },
            method);
    }

    void addModRewriteRule(std::string oldPrefix, std::string newPrefix)
    {
        NX_DEBUG(this, "New rewrite rule '%1*' to '%2*'", oldPrefix, newPrefix);
        m_rewritePrefixes.emplace(std::move(oldPrefix), std::move(newPrefix));
    }

    void clear()
    {
        m_factories.clear();
        m_rewritePrefixes.clear();
    }

private:
    using FactoryFunc = std::function<std::unique_ptr<RequestHandlerWithContext>()>;

    struct HandlerFactory
    {
        FactoryFunc func;

        operator bool() const { return static_cast<bool>(func); }

        std::optional<HandlerContext> instantiate(
            RequestPathParams pathParams,
            std::string pathTemplate) const
        {
            HandlerContext handlerContext{
                func(),
                std::move(pathTemplate)
            };
            handlerContext.handler->setRequestPathParams(std::move(pathParams));
            return handlerContext;
        }
    };

    struct PathMatchContext
    {
        HandlerFactory defaultHandlerFactory;
        PathMatcherType<HandlerFactory> pathToFactory;
    };

    std::map<std::string, std::string> m_rewritePrefixes;
    std::map<Method, PathMatchContext> m_factories;

    virtual void applyModRewrite(nx::utils::Url* url) const override
    {
        if (const auto it = findByMaxPrefix(m_rewritePrefixes, url->path().toStdString()))
        {
            const auto newPath = url->path().replace(it->first.c_str(), it->second.c_str());
            NX_VERBOSE(this, "Rewriting url '%1' to '%2'", url->path(), newPath);
            url->setPath(newPath);
        }
    }

    virtual std::optional<HandlerContext> getHandler(
        const Method& method,
        const std::string& path) const override
    {
        auto methodFactory = m_factories.find(method);
        if (methodFactory != m_factories.end())
        {
            auto handlerContext = matchPath(methodFactory->second, path);
            if (handlerContext)
                return handlerContext;
        }

        auto anyMethodFactory = m_factories.find(Method(kAnyMethod));
        if (anyMethodFactory != m_factories.end())
            return matchPath(anyMethodFactory->second, path);

        return std::nullopt;
    }

    std::optional<HandlerContext> matchPath(
        const PathMatchContext& pathMatchContext,
        const std::string& path) const
    {
        RequestPathParams pathParams;
        std::string pathTemplate;

        std::optional<typename PathMatcherType<HandlerFactory>::MatchResult> result =
            pathMatchContext.pathToFactory.match(path);

        if (result)
            return result->value.instantiate(std::move(result->pathParams), std::string(result->pathTemplate));

        if (pathMatchContext.defaultHandlerFactory)
        {
            return pathMatchContext.defaultHandlerFactory.instantiate(
                std::move(pathParams),
                std::move(pathTemplate));
        }

        return std::nullopt;
    }
};

//-------------------------------------------------------------------------------------------------

class MessageDispatcher:
    public BasicMessageDispatcher<ExactPathMatcher>
{
};

} // namespace nx::network::http
