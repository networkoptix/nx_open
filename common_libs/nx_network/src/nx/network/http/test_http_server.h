#pragma once

#include <memory>

#include <QtCore/QString>

#include <nx/network/connection_server/multi_address_server.h>

#include "server/http_message_dispatcher.h"
#include "server/http_server_base_authentication_manager.h"
#include "server/http_server_plain_text_credentials_provider.h"
#include "server/http_stream_socket_server.h"
#include "server/handler/http_server_handler_custom.h"
#include "server/rest/http_server_rest_message_dispatcher.h"

//-------------------------------------------------------------------------------------------------

class TestAuthenticationManager:
    public nx_http::server::BaseAuthenticationManager
{
    typedef nx_http::server::BaseAuthenticationManager BaseType;

public:
    TestAuthenticationManager(
        nx_http::server::AbstractAuthenticationDataProvider* authenticationDataProvider);

    virtual void authenticate(
        const nx_http::HttpServerConnection& connection,
        const nx_http::Request& request,
        nx_http::server::AuthenticationCompletionHandler completionHandler) override;

    void setAuthenticationEnabled(bool value);

private:
    bool m_authenticationEnabled;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API TestHttpServer
{
public:
    using ProcessHttpRequestFunc = nx::utils::MoveOnlyFunc<void(
        nx_http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler /*completionHandler*/)>;

    TestHttpServer();
    ~TestHttpServer();

    bool bindAndListen(const SocketAddress& endpoint = SocketAddress::anyPrivateAddress);
    SocketAddress serverAddress() const;

    template<typename RequestHandlerType>
    bool registerRequestProcessor(
        const QString& path,
        std::function<std::unique_ptr<RequestHandlerType>()> factoryFunc )
    {
        return m_httpMessageDispatcher.registerRequestProcessor(
            path,
            std::move(factoryFunc) );
    }

    template<typename RequestHandlerType>
    bool registerRequestProcessor(const QString& path)
    {
        return m_httpMessageDispatcher.registerRequestProcessor<RequestHandlerType>(
            path,
            []() -> std::unique_ptr<RequestHandlerType>
            {
                return std::make_unique<RequestHandlerType>();
            });
    }

    template<typename Func>
    bool registerRequestProcessorFunc(const QString& path, Func func)
    {
        using RequestHandlerType = nx_http::server::handler::CustomRequestHandler<const Func&>;

        return m_httpMessageDispatcher.registerRequestProcessor<RequestHandlerType>(
            path,
            [func = std::move(func)]() -> std::unique_ptr<RequestHandlerType>
            {
                return std::make_unique<RequestHandlerType>(func);
            });
    }

    bool registerStaticProcessor(
        const QString& path,
        QByteArray msgBody,
        const nx_http::StringType& mimeType);

    bool registerFileProvider(
        const QString& httpPath,
        const QString& filePath,
        const nx_http::StringType& mimeType);

    bool registerRedirectHandler(
        const QString& resourcePath,
        const QUrl& location);

    // used for test purpose
    void setPersistentConnectionEnabled(bool value);
    void addModRewriteRule(QString oldPrefix, QString newPrefix);

    void setAuthenticationEnabled(bool value);
    void registerUserCredentials(const nx::String& userName, const nx::String& password);

    nx_http::HttpStreamSocketServer& server() { return *m_httpServer; }

private:
    nx_http::server::rest::MessageDispatcher m_httpMessageDispatcher;
    nx_http::server::PlainTextCredentialsProvider m_credentialsProvider;
    TestAuthenticationManager m_authenticationManager;
    std::unique_ptr<nx_http::HttpStreamSocketServer> m_httpServer;
};

//-------------------------------------------------------------------------------------------------
// class RandomlyFailingHttpConnection

class NX_NETWORK_API RandomlyFailingHttpConnection:
    public nx_http::BaseConnection<RandomlyFailingHttpConnection>,
    public std::enable_shared_from_this<RandomlyFailingHttpConnection>
{
public:
    using BaseType = nx_http::BaseConnection<RandomlyFailingHttpConnection>;

    RandomlyFailingHttpConnection(
        nx::network::server::StreamConnectionHolder<RandomlyFailingHttpConnection>* socketServer,
        std::unique_ptr<AbstractStreamSocket> sock);
    virtual ~RandomlyFailingHttpConnection();

    void setResponseBuffer(const QByteArray& buf);

    void processMessage(nx_http::Message request);

private:
    QByteArray m_responseBuffer;
    int m_requestsToAnswer;

    void onResponseSent(SystemError::ErrorCode sysErrorCode);
};

class NX_NETWORK_API RandomlyFailingHttpServer:
    public nx::network::server::StreamSocketServer<
        RandomlyFailingHttpServer, RandomlyFailingHttpConnection>
{
    using base_type = nx::network::server::StreamSocketServer<
        RandomlyFailingHttpServer, RandomlyFailingHttpConnection>;

public:
    RandomlyFailingHttpServer(
        bool sslRequired,
        nx::network::NatTraversalSupport natTraversalSupport);

    void setResponseBuffer(const QByteArray& buf);

private:
    QByteArray m_responseBuffer;

    virtual std::shared_ptr<RandomlyFailingHttpConnection> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override;
};
