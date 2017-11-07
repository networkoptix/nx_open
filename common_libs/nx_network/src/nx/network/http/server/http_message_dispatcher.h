#pragma once

#include <memory>

#include <boost/optional.hpp>

#include <nx/network/connection_server/message_dispatcher.h>
#include <nx/utils/std/cpp14.h>

#include "abstract_http_request_handler.h"
#include "http_server_exact_path_matcher.h"
#include "http_server_connection.h"

namespace nx_http {

static const nx_http::StringType kAnyMethod;
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
        nx_http::Message message,
        nx::utils::stree::ResourceContainer authInfo,
        CompletionFuncRefType completionFunc) const
    {
        NX_ASSERT(message.type == nx_http::MessageType::request);
        nx_http::RequestLine& request = message.request->requestLine;
        applyModRewrite(&request.url);

        auto handler = getHandler(request.method, request.url.path());
        if (!handler)
            return false;

        const auto handlerPtr = handler.get();
        return handlerPtr->processRequest(
            connection, std::move(message), std::move(authInfo),
            [handler = std::move(handler), completionFunc = std::move(completionFunc)](
                nx_http::Message message,
                std::unique_ptr<nx_http::AbstractMsgBodySource> bodySource,
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
        const nx_http::StringType& method = kAnyMethod)
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
        
        return pathMatchContext.pathToFactory.add(path.toUtf8(), std::move(factoryFunc));
    }

    template<typename RequestHandlerType>
    bool registerRequestProcessor(
        const QString& path = kAnyPath,
        const nx_http::StringType& method = kAnyMethod)
    {
        return registerRequestProcessor<RequestHandlerType>(
            path,
            []() { return std::make_unique<RequestHandlerType>(); },
            method);
    }

    void addModRewriteRule(QString oldPrefix, QString newPrefix)
    {
        NX_LOGX(lm("New rewrite rule '%1*' to '%2*'").args(oldPrefix, newPrefix), cl_logDEBUG1);
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
    std::map<nx_http::StringType /*method*/, PathMatchContext> m_factories;

    virtual void applyModRewrite(nx::utils::Url* url) const override
    {
        if (const auto it = findByMaxPrefix(m_rewritePrefixes, url->path()))
        {
            const auto newPath = url->path().replace(it->first, it->second);
            NX_LOGX(lm("Rewriting url '%1' to '%2'").args(url->path(), newPath), cl_logDEBUG2);
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
        std::vector<StringType> pathParams;
        boost::optional<const FactoryFunc&> factory =
            pathMatchContext.pathToFactory.match(path.toUtf8(), &pathParams);
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

} // namespace nx_http
