#pragma once

#include <memory>

#include <boost/optional.hpp>

#include <nx/network/connection_server/message_dispatcher.h>
#include <nx/utils/std/cpp14.h>

#include "abstract_http_request_handler.h"
#include "handler/http_server_handler_custom.h"
#include "http_server_exact_path_matcher.h"
#include "http_server_connection.h"

namespace nx {
namespace network {
namespace http {

static const nx::network::http::StringType kAnyMethod;
static const QString kAnyPath;

template<typename Value>
const std::pair<const QString, Value>* findByMaxPrefix(
    const std::map<QString, Value>& map, const QString& key)
{
    auto it = map.upper_bound(key);
    if (it == map.begin())
        return nullptr;

    --it;
    if (!key.startsWith(it->first))
        return nullptr;

    return &(*it);
}

class AbstractMessageDispatcher
{
public:
    virtual ~AbstractMessageDispatcher() = default;

    /**
     * Pass message to corresponding processor.
     *
     *  @param message This object is not moved in case of failure to find processor.
     *  @return true if request processing passed to corresponding processor and async processing
     *      has been started,  false otherwise.
     */
    template<class CompletionFuncRefType>
    bool dispatchRequest(
        HttpServerConnection* const connection,
        nx::network::http::Request request,
        nx::utils::stree::ResourceContainer authInfo,
        CompletionFuncRefType completionFunc) const
    {
        applyModRewrite(&request.requestLine.url);

        auto handler = getHandler(
            request.requestLine.method,
            request.requestLine.url.path());
        if (!handler)
            return false;

        const auto handlerPtr = handler.get();
        return handlerPtr->processRequest(
            connection, std::move(request), std::move(authInfo),
            [handler = std::move(handler), completionFunc = std::move(completionFunc)](
                nx::network::http::Message message,
                std::unique_ptr<nx::network::http::AbstractMsgBodySource> bodySource,
                ConnectionEvents connectionEvents) mutable
            {
                completionFunc(
                    std::move(message),
                    std::move(bodySource),
                    std::move(connectionEvents));

                handler.reset();
            });
    }

protected:
    virtual void applyModRewrite(nx::utils::Url* url) const = 0;
    virtual std::unique_ptr<AbstractHttpRequestHandler> getHandler(
        const StringType& method,
        const QString& path) const = 0;
};

template<template<typename> class PathMatcherType>
class BasicMessageDispatcher:
    public AbstractMessageDispatcher
{
public:
    virtual ~BasicMessageDispatcher() = default;

    /**
     * @return false if some handler is already registered for path.
     */
    template<typename RequestHandlerType>
    bool registerRequestProcessor(
        const QString& path,
        std::function<std::unique_ptr<RequestHandlerType>()> factoryFunc,
        const nx::network::http::StringType& method = kAnyMethod)
    {
        NX_ASSERT(factoryFunc);
        PathMatchContext& pathMatchContext = m_factories[method];
        if (path == kAnyPath)
        {
            if (pathMatchContext.defaultFactory)
                return false;
            pathMatchContext.defaultFactory = std::move(factoryFunc);
            return true;
        }

        return pathMatchContext.pathToFactory.add(
            path.toStdString(), std::move(factoryFunc));
    }

    template<typename RequestHandlerType>
    bool registerRequestProcessor(
        const QString& path = kAnyPath,
        const nx::network::http::StringType& method = kAnyMethod)
    {
        return registerRequestProcessor<RequestHandlerType>(
            path,
            []() { return std::make_unique<RequestHandlerType>(); },
            method);
    }

    template<typename Func>
    bool registerRequestProcessorFunc(
        const nx::network::http::StringType& method,
        const std::string& path,
        Func func)
    {
        using Handler = server::handler::CustomRequestHandler<Func>;

        return registerRequestProcessor<Handler>(
            path.c_str(),
            [func = std::move(func)]() { return std::make_unique<Handler>(func); },
            method);
    }

    void addModRewriteRule(QString oldPrefix, QString newPrefix)
    {
        NX_DEBUG(this, lm("New rewrite rule '%1*' to '%2*'").args(oldPrefix, newPrefix));
        m_rewritePrefixes.emplace(std::move(oldPrefix), std::move(newPrefix));
    }

private:
    using FactoryFunc = std::function<std::unique_ptr<AbstractHttpRequestHandler>()>;

    struct PathMatchContext
    {
        FactoryFunc defaultFactory;
        PathMatcherType<FactoryFunc> pathToFactory;
    };

    std::map<QString, QString> m_rewritePrefixes;
    std::map<nx::network::http::StringType /*method*/, PathMatchContext> m_factories;

    virtual void applyModRewrite(nx::utils::Url* url) const override
    {
        if (const auto it = findByMaxPrefix(m_rewritePrefixes, url->path()))
        {
            const auto newPath = url->path().replace(it->first, it->second);
            NX_VERBOSE(this, lm("Rewriting url '%1' to '%2'").args(url->path(), newPath));
            url->setPath(newPath);
        }
    }

    virtual std::unique_ptr<AbstractHttpRequestHandler> getHandler(
        const StringType& method,
        const QString& path) const override
    {
        auto methodFactory = m_factories.find(method);
        if (methodFactory != m_factories.end())
        {
            auto handler = matchPath(methodFactory->second, path);
            if (handler)
                return handler;
        }

        auto anyMethodFactory = m_factories.find(kAnyMethod);
        if (anyMethodFactory != m_factories.end())
            return matchPath(anyMethodFactory->second, path);

        return nullptr;
    }

    std::unique_ptr<AbstractHttpRequestHandler> matchPath(
        const PathMatchContext& pathMatchContext,
        const QString& path) const
    {
        RequestPathParams pathParams;
        boost::optional<const FactoryFunc&> factory =
            pathMatchContext.pathToFactory.match(path.toStdString(), &pathParams);
        if (factory)
        {
            auto handler = (*factory)();
            handler->setRequestPathParams(std::move(pathParams));
            return handler;
        }

        if (pathMatchContext.defaultFactory)
            return pathMatchContext.defaultFactory();

        return nullptr;
    }
};

class MessageDispatcher:
    public BasicMessageDispatcher<ExactPathMatcher>
{
};

} // namespace nx
} // namespace network
} // namespace http
