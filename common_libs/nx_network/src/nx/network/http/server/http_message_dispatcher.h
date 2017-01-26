#pragma once

#include <memory>

#include <nx/network/connection_server/message_dispatcher.h>
#include <nx/utils/std/cpp14.h>

#include "abstract_http_request_handler.h"
#include "http_server_connection.h"

namespace nx_http {

static const nx_http::StringType kAnyMethod;
static const QString kAnyPath;

// TODO: #ak This class MUST search processor by max string prefix.
class NX_NETWORK_API MessageDispatcher
{
public:
    virtual ~MessageDispatcher() = default;

    template<typename RequestHandlerType>
    bool registerRequestProcessor(
        const QString& path,
        std::function<std::unique_ptr<RequestHandlerType>()> factoryFunc,
        const nx_http::StringType& method = kAnyMethod)
    {
        NX_ASSERT(factoryFunc);
        return m_factories[method].emplace(path, std::move(factoryFunc)).second;
    }

    template<typename RequestHandlerType>
    bool registerRequestProcessor(
        const QString& path = kAnyPath, const nx_http::StringType& method = kAnyMethod)
    {
        return registerRequestProcessor<RequestHandlerType>(
            path,
            []() { return std::make_unique<RequestHandlerType>(); },
            method);
    }

    void addModRewriteRule(QString oldPrefix, QString newPrefix);

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
        stree::ResourceContainer authInfo,
        CompletionFuncRefType completionFunc) const
    {
        NX_ASSERT(message.type == nx_http::MessageType::request);
        nx_http::RequestLine& request = message.request->requestLine;
        applyModRewrite(&request.url);

        auto handler = makeHandler(request.method, request.url.path());
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

private:
    void applyModRewrite(QUrl* url) const;
    std::unique_ptr<AbstractHttpRequestHandler> makeHandler(
        const StringType& method, const QString& path) const;

    std::map<QString, QString> m_rewritePrefixes;
    std::map<nx_http::StringType /*method*/, std::map<
        QString /*path*/, std::function<std::unique_ptr<AbstractHttpRequestHandler>()>
    >> m_factories;
};

} // namespace nx_http
