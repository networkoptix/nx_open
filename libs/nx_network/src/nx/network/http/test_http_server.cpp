// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_http_server.h"

#include <fstream>
#include <sstream>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/server/handler/http_server_handler_redirect.h>
#include <nx/network/http/server/handler/http_server_handler_static_data.h>
#include <nx/utils/random.h>

namespace nx::network::http {

TestHttpServer::~TestHttpServer()
{
    if (m_httpServer)
        terminateListener();

    NX_INFO(this, "Stopped");
}

bool TestHttpServer::bindAndListen(const SocketAddress& endpoint)
{
    if (!m_httpServer->bind(endpoint))
        return false;

    if (!m_httpServer->listen())
        return false;

    NX_INFO(this, nx::format("Started on %1").arg(m_httpServer->address()));
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

void TestHttpServer::addModRewriteRule(std::string oldPrefix, std::string newPrefix)
{
    m_httpMessageDispatcher.addModRewriteRule(std::move(oldPrefix), std::move(newPrefix));
}

void TestHttpServer::enableAuthentication(const std::string& path)
{
    m_authDispatcher.add(std::regex(path), &m_authenticationManager);
}

void TestHttpServer::registerUserCredentials(const Credentials& credentials)
{
    m_credentialsProvider.addCredentials(credentials);
}

void TestHttpServer::enableAuthentication(
    const std::string& pathRegex,
    server::AbstractAuthenticationManager& mgr)
{
    m_authDispatcher.add(std::regex(pathRegex), &mgr);
}

bool TestHttpServer::registerStaticProcessor(
    const std::string& path,
    nx::Buffer msgBody,
    const std::string& mimeType,
    const Method& method)
{
    return registerRequestProcessor(
        path, [=]()
        {
            return std::make_unique<nx::network::http::server::handler::StaticData>(
                mimeType,
                std::move(msgBody));
        },
        method);
}

bool TestHttpServer::registerFileProvider(
    const std::string& httpPath,
    const std::string& filePath,
    const std::string& mimeType,
    const Method& method)
{
    std::ifstream contentFile(filePath, std::ios_base::in | std::ios_base::binary);
    if (!contentFile.is_open())
        return false;

    std::stringstream s;
    s << contentFile.rdbuf();
    return registerStaticProcessor(
        httpPath,
        nx::Buffer(s.str()),
        mimeType,
        method);
}

bool TestHttpServer::registerContentProvider(
    const std::string& httpPath,
    ContentProviderFactoryFunction contentProviderFactory)
{
    auto contentProviderFactoryShared =
        std::make_shared<ContentProviderFactoryFunction>(std::move(contentProviderFactory));

    return registerRequestProcessorFunc(
        httpPath,
        [contentProviderFactoryShared](
            nx::network::http::RequestContext /*requestContext*/,
            nx::network::http::RequestProcessedHandler completionHandler)
        {
            auto msgBody = (*contentProviderFactoryShared)();
            if (msgBody)
            {
                completionHandler(
                    nx::network::http::RequestResult(
                        nx::network::http::StatusCode::ok,
                        std::move(msgBody)));
            }
            else
            {
                completionHandler(nx::network::http::StatusCode::internalServerError);
            }
        },
        nx::network::http::Method::get);
}

bool TestHttpServer::registerRedirectHandler(
    const std::string& resourcePath,
    const nx::utils::Url& location,
    const Method& method)
{
    return registerRequestProcessor(
        resourcePath,
        [location]() -> std::unique_ptr<nx::network::http::server::handler::Redirect>
        {
            return std::make_unique<nx::network::http::server::handler::Redirect>(location);
        },
        method);
}

void TestHttpServer::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_httpServer->bindToAioThread(aioThread);
}

void TestHttpServer::terminateListener()
{
    m_httpServer->pleaseStopSync();
    m_httpMessageDispatcher.waitUntilAllRequestsCompleted();
    m_httpServer.reset();
}

void TestHttpServer::installIntermediateRequestHandler(
    std::unique_ptr<IntermediaryHandler> handler)
{
    auto bak = m_authDispatcher.nextHandler();
    m_authDispatcher.setNextHandler(handler.get());
    handler->setNextHandler(bak);

    m_installedIntermediateRequestHandlers.push_back(std::move(handler));
}

//-------------------------------------------------------------------------------------------------
// class RandomlyFailingHttpConnection

RandomlyFailingHttpConnection::RandomlyFailingHttpConnection(
    std::unique_ptr<AbstractStreamSocket> sock)
    :
    BaseType(std::move(sock)),
    m_requestsToAnswer(nx::utils::random::number<int>(0, 3))
{
}

RandomlyFailingHttpConnection::~RandomlyFailingHttpConnection()
{
}

void RandomlyFailingHttpConnection::setResponseBuffer(const nx::Buffer& buf)
{
    m_responseBuffer = buf;
}

void RandomlyFailingHttpConnection::processMessage(nx::network::http::Message /*request*/)
{
    using namespace std::placeholders;

    nx::Buffer dataToSend;
    if (m_requestsToAnswer > 0)
    {
        dataToSend = m_responseBuffer;
        --m_requestsToAnswer;
    }
    else
    {
        const auto bytesToSend = nx::utils::random::number<std::size_t>(0, m_responseBuffer.size());
        if (bytesToSend == 0)
            return closeConnection(SystemError::noError);
        dataToSend = m_responseBuffer.substr(0, bytesToSend);
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

RandomlyFailingHttpServer::~RandomlyFailingHttpServer()
{
    pleaseStopSync();
}

void RandomlyFailingHttpServer::setResponseBuffer(const nx::Buffer& buf)
{
    m_responseBuffer = buf;
}

std::shared_ptr<RandomlyFailingHttpConnection> RandomlyFailingHttpServer::createConnection(
    std::unique_ptr<AbstractStreamSocket> _socket)
{
    auto result = std::make_shared<RandomlyFailingHttpConnection>(std::move(_socket));
    result->setResponseBuffer(m_responseBuffer);
    return result;
}

} // namespace nx::network::http
