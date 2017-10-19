#include "test_http_server.h"

#include <QtCore/QFile>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/server/handler/http_server_handler_redirect.h>
#include <nx/network/http/server/handler/http_server_handler_static_data.h>
#include <nx/utils/random.h>

//-------------------------------------------------------------------------------------------------

TestAuthenticationManager::TestAuthenticationManager(
    nx_http::server::AbstractAuthenticationDataProvider* authenticationDataProvider)
    :
    BaseType(authenticationDataProvider),
    m_authenticationEnabled(false)
{
}

void TestAuthenticationManager::authenticate(
    const nx_http::HttpServerConnection& connection,
    const nx_http::Request& request,
    nx_http::server::AuthenticationCompletionHandler completionHandler)
{
    if (m_authenticationEnabled)
    {
        BaseType::authenticate(connection, request, std::move(completionHandler));
    }
    else
    {
        completionHandler(nx_http::server::AuthenticationResult(
            true,
            nx::utils::stree::ResourceContainer(),
            boost::none,
            nx_http::HttpHeaders(),
            nullptr));
    }
}

void TestAuthenticationManager::setAuthenticationEnabled(bool value)
{
    m_authenticationEnabled = value;
}

//-------------------------------------------------------------------------------------------------

TestHttpServer::~TestHttpServer()
{
    m_httpServer->pleaseStopSync();
    NX_LOGX("Stopped", cl_logINFO);
}

bool TestHttpServer::bindAndListen(const SocketAddress& endpoint)
{
    if (!m_httpServer->bind(endpoint))
        return false;

    if (!m_httpServer->listen())
        return false;

    NX_LOGX(lm("Started on %1").arg(m_httpServer->address()), cl_logINFO);
    return true;
}

SocketAddress TestHttpServer::serverAddress() const
{
    auto socketAddress = m_httpServer->address();
    if (socketAddress.address == HostAddress::anyHost)
        socketAddress.address = HostAddress::localhost;
    return socketAddress;
}

void TestHttpServer::setPersistentConnectionEnabled(bool value)
{
    m_httpServer->setPersistentConnectionEnabled(value);
}

void TestHttpServer::addModRewriteRule(QString oldPrefix, QString newPrefix)
{
    m_httpMessageDispatcher.addModRewriteRule(std::move(oldPrefix), std::move(newPrefix));
}

void TestHttpServer::setAuthenticationEnabled(bool value)
{
    m_authenticationManager.setAuthenticationEnabled(value);
}

void TestHttpServer::registerUserCredentials(
    const nx::String& userName,
    const nx::String& password)
{
    m_credentialsProvider.addCredentials(userName, password);
}

bool TestHttpServer::registerStaticProcessor(
    const QString& path,
    QByteArray msgBody,
    const nx_http::StringType& mimeType,
    nx_http::StringType method)
{
    return registerRequestProcessor<nx_http::server::handler::StaticData>(
        path, [=]() -> std::unique_ptr<nx_http::server::handler::StaticData>
        {
            return std::make_unique<nx_http::server::handler::StaticData>(
                mimeType,
                std::move(msgBody));
        },
        method);
}

bool TestHttpServer::registerFileProvider(
    const QString& httpPath,
    const QString& filePath,
    const nx_http::StringType& mimeType,
    nx_http::StringType method)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const auto fileContents = f.readAll();
    return registerStaticProcessor(
        httpPath,
        std::move(fileContents),
        mimeType,
        method);
}

bool TestHttpServer::registerContentProvider(
    const QString& httpPath,
    ContentProviderFactoryFunction contentProviderFactory)
{
    auto contentProviderFactoryShared = 
        std::make_shared<ContentProviderFactoryFunction>(std::move(contentProviderFactory));

    return registerRequestProcessorFunc(
        httpPath,
        [contentProviderFactoryShared](
            nx_http::HttpServerConnection* const /*connection*/,
            nx::utils::stree::ResourceContainer /*authInfo*/,
            nx_http::Request /*request*/,
            nx_http::Response* const /*response*/,
            nx_http::RequestProcessedHandler completionHandler)
        {
            auto msgBody = (*contentProviderFactoryShared)();
            if (msgBody)
            {
                completionHandler(
                    nx_http::RequestResult(nx_http::StatusCode::ok, std::move(msgBody)));
            }
            else
            {
                completionHandler(nx_http::StatusCode::internalServerError);
            }
        },
        nx_http::Method::get);
}

bool TestHttpServer::registerRedirectHandler(
    const QString& resourcePath,
    const QUrl& location,
    nx_http::StringType method)
{
    return registerRequestProcessor<nx_http::server::handler::Redirect>(
        resourcePath,
        [location]() -> std::unique_ptr<nx_http::server::handler::Redirect>
        {
            return std::make_unique<nx_http::server::handler::Redirect>(location);
        },
        method);
}

//-------------------------------------------------------------------------------------------------
// class RandomlyFailingHttpConnection

RandomlyFailingHttpConnection::RandomlyFailingHttpConnection(
    nx::network::server::StreamConnectionHolder<RandomlyFailingHttpConnection>* socketServer,
    std::unique_ptr<AbstractStreamSocket> sock)
    :
    BaseType(socketServer, std::move(sock)),
    m_requestsToAnswer(nx::utils::random::number<int>(0, 3))
{
}

RandomlyFailingHttpConnection::~RandomlyFailingHttpConnection()
{
}

void RandomlyFailingHttpConnection::setResponseBuffer(const QByteArray& buf)
{
    m_responseBuffer = buf;
}

void RandomlyFailingHttpConnection::processMessage(nx_http::Message /*request*/)
{
    using namespace std::placeholders;

    QByteArray dataToSend;
    if (m_requestsToAnswer > 0)
    {
        dataToSend = m_responseBuffer;
        --m_requestsToAnswer;
    }
    else
    {
        const auto bytesToSend = nx::utils::random::number<int>(0, m_responseBuffer.size());
        if (bytesToSend == 0)
            return closeConnection(SystemError::noError);
        dataToSend = m_responseBuffer.left(bytesToSend);
    }

    sendData(
        dataToSend,
        std::bind(&RandomlyFailingHttpConnection::onResponseSent, this, _1));
}

void RandomlyFailingHttpConnection::onResponseSent(
    SystemError::ErrorCode /*sysErrorCode*/)
{
    //closeConnection(sysErrorCode);
}

//-------------------------------------------------------------------------------------------------
// class RandomlyFailingHttpServer

RandomlyFailingHttpServer::RandomlyFailingHttpServer(
    bool sslRequired,
    nx::network::NatTraversalSupport natTraversalSupport)
    :
    base_type(sslRequired, natTraversalSupport)
{
}

void RandomlyFailingHttpServer::setResponseBuffer(const QByteArray& buf)
{
    m_responseBuffer = buf;
}

std::shared_ptr<RandomlyFailingHttpConnection> RandomlyFailingHttpServer::createConnection(
    std::unique_ptr<AbstractStreamSocket> _socket)
{
    auto result = std::make_shared<RandomlyFailingHttpConnection>(
        this,
        std::move(_socket));
    result->setResponseBuffer(m_responseBuffer);
    return result;
}
