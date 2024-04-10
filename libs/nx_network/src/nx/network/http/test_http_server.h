// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <string>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/ssl/context.h>

#include "server/authentication_dispatcher.h"
#include "server/handler/http_server_handler_custom.h"
#include "server/http_message_dispatcher.h"
#include "server/http_server_authentication_manager.h"
#include "server/http_server_plain_text_credentials_provider.h"
#include "server/http_stream_socket_server.h"
#include "server/rest/http_server_rest_message_dispatcher.h"

namespace nx::network::http {

using ContentProviderFactoryFunction =
    nx::utils::MoveOnlyFunc<std::unique_ptr<nx::network::http::AbstractMsgBodySource>()>;

class NX_NETWORK_API TestHttpServer
{
public:
    using ProcessHttpRequestFunc = nx::utils::MoveOnlyFunc<void(
        nx::network::http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::AttributeDictionary /*authInfo*/,
        nx::network::http::Request /*request*/,
        nx::network::http::Response* const /*response*/,
        nx::network::http::RequestProcessedHandler /*completionHandler*/)>;

    TestHttpServer(server::Role role = server::Role::resourceServer):
        TestHttpServer(role, ssl::Context::instance())
    {
    }

    template<typename... Args>
    TestHttpServer(server::Role role, Args&&... args):
        m_authenticationManager(&m_httpMessageDispatcher, &m_credentialsProvider, role),
        m_authDispatcher(&m_httpMessageDispatcher)
    {
        m_httpServer = std::make_unique<nx::network::http::HttpStreamSocketServer>(
            &m_authDispatcher,
            std::forward<Args>(args)...);
    }

    ~TestHttpServer();

    bool bindAndListen(const SocketAddress& endpoint = SocketAddress::anyPrivateAddress);
    SocketAddress serverAddress() const;

    void bindToAioThread(aio::AbstractAioThread* aioThread);

    template<typename HandlerFactoryFunc>
    bool registerRequestProcessor(
        const std::string& path,
        HandlerFactoryFunc&& factoryFunc,
        const Method& method = nx::network::http::kAnyMethod)
    {
        return m_httpMessageDispatcher.registerRequestProcessor(
            path,
            std::forward<HandlerFactoryFunc>(factoryFunc),
            method);
    }

    template<typename RequestHandlerType>
    bool registerRequestProcessor(
        const std::string& path,
        const Method& method = nx::network::http::kAnyMethod)
    {
        return m_httpMessageDispatcher.registerRequestProcessor(
            path,
            []() { return std::make_unique<RequestHandlerType>(); },
            method);
    }

    template<typename Func>
    bool registerRequestProcessorFunc(
        const std::string& path,
        Func func,
        const Method& method = nx::network::http::kAnyMethod,
        MessageBodyDeliveryType type = MessageBodyDeliveryType::buffer)
    {
        return m_httpMessageDispatcher.registerRequestProcessorFunc(
            method,
            path,
            std::move(func),
            type);
    }

    bool registerStaticProcessor(
        const std::string& path,
        nx::Buffer msgBody,
        const std::string& mimeType,
        const Method& method = nx::network::http::kAnyMethod);

    bool registerFileProvider(
        const std::string& httpPath,
        const std::string& filePath,
        const std::string& mimeType,
        const Method& method = nx::network::http::kAnyMethod);

    bool registerContentProvider(
        const std::string& httpPath,
        ContentProviderFactoryFunction contentProviderFactory);

    bool registerRedirectHandler(
        const std::string& resourcePath,
        const nx::utils::Url& location,
        const Method& method = nx::network::http::kAnyMethod);

    // used for test purpose
    void setPersistentConnectionEnabled(bool value);
    void addModRewriteRule(std::string oldPrefix, std::string newPrefix);

    server::AuthenticationDispatcher& authDispatcher() { return m_authDispatcher; }

    /**
     * Enables internal authentication manager to authenticate request under path.
     */
    void enableAuthentication(const std::string& pathRegex);
    void registerUserCredentials(const Credentials& credentials);

    /**
     * Enables external authentication manager to authenticate request under path.
     */
    void enableAuthentication(
        const std::string& pathRegex,
        server::AbstractAuthenticationManager& mgr);

    nx::network::http::HttpStreamSocketServer& server() { return *m_httpServer; }
    const nx::network::http::HttpStreamSocketServer& server() const { return *m_httpServer; }
    nx::network::http::server::rest::MessageDispatcher& httpMessageDispatcher()
        { return m_httpMessageDispatcher; }

    void terminateListener();
    void pleaseStopSync();

    // Installs handler to receive all incoming HTTP request.
    // handler->setNextHandler() receives the previous default handler.
    void installIntermediateRequestHandler(std::unique_ptr<IntermediaryHandler> handler);

private:
    nx::network::http::server::rest::MessageDispatcher m_httpMessageDispatcher;
    nx::network::http::server::PlainTextCredentialsProvider m_credentialsProvider;
    server::AuthenticationManager m_authenticationManager;
    std::vector<std::unique_ptr<IntermediaryHandler>> m_installedIntermediateRequestHandlers;
    server::AuthenticationDispatcher m_authDispatcher;
    std::unique_ptr<nx::network::http::HttpStreamSocketServer> m_httpServer;
};

//-------------------------------------------------------------------------------------------------
// class RandomlyFailingHttpConnection

class NX_NETWORK_API RandomlyFailingHttpConnection:
    public nx::network::http::deprecated::BaseConnection<RandomlyFailingHttpConnection>,
    public std::enable_shared_from_this<RandomlyFailingHttpConnection>
{
public:
    using BaseType = nx::network::http::deprecated::BaseConnection<RandomlyFailingHttpConnection>;

    RandomlyFailingHttpConnection(std::unique_ptr<AbstractStreamSocket> sock);
    virtual ~RandomlyFailingHttpConnection();

    void setResponseBuffer(const nx::Buffer& buf);

protected:
    virtual void processMessage(nx::network::http::Message request) override;

private:
    nx::Buffer m_responseBuffer;
    int m_requestsToAnswer;

    void onResponseSent(SystemError::ErrorCode sysErrorCode);
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API RandomlyFailingHttpServer:
    public nx::network::server::StreamSocketServer<
        RandomlyFailingHttpServer, RandomlyFailingHttpConnection>
{
    using base_type = nx::network::server::StreamSocketServer<
        RandomlyFailingHttpServer, RandomlyFailingHttpConnection>;

public:
    template<typename... Args>
    RandomlyFailingHttpServer(Args&&... args):
        base_type(std::forward<Args>(args)...)
    {
    }

    virtual ~RandomlyFailingHttpServer() override;

    void setResponseBuffer(const nx::Buffer& buf);

private:
    nx::Buffer m_responseBuffer;

    virtual std::shared_ptr<RandomlyFailingHttpConnection> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override;
};

} // namespace nx::network::http
