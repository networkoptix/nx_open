/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include "test_http_server.h"

#include <QtCore/QFile>

#include <nx/network/http/buffer_source.h>
#include <nx/utils/random.h>

TestHttpServer::TestHttpServer()
{
    m_httpServer.reset(
        new nx_http::HttpStreamSocketServer(
            nullptr,
            &m_httpMessageDispatcher,
            true,
            nx::network::NatTraversalSupport::disabled ) );
}

TestHttpServer::~TestHttpServer()
{
    m_httpServer->pleaseStop();
}

bool TestHttpServer::bindAndListen()
{
    return m_httpServer->bind( SocketAddress( HostAddress::localhost, 0 ) )
        && m_httpServer->listen();
}

SocketAddress TestHttpServer::serverAddress() const
{
    return m_httpServer->address();
}

class StaticHandler
:
    public nx_http::AbstractHttpRequestHandler
{
public:
    StaticHandler(const nx_http::StringType& mimeType, QByteArray response)
    :
        m_mimeType(mimeType),
        m_response(std::move(response))
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler )
    {
        completionHandler(
            nx_http::RequestResult(
                nx_http::StatusCode::ok,
                std::make_unique< nx_http::BufferSource >(m_mimeType, m_response)));
    }

private:
    const nx_http::StringType m_mimeType;
    const QByteArray m_response;
};

void TestHttpServer::setPersistentConnectionEnabled(bool value)
{
    m_httpServer->setPersistentConnectionEnabled(value);
}

bool TestHttpServer::registerStaticProcessor(
    const QString& path,
    QByteArray msgBody,
    const nx_http::StringType& mimeType)
{
    return registerRequestProcessor< StaticHandler >(
        path, [ = ]() -> std::unique_ptr< StaticHandler >
        {
            return std::make_unique< StaticHandler >(
                mimeType,
                std::move(msgBody) );
        } );
}

bool TestHttpServer::registerFileProvider(
    const QString& httpPath,
    const QString& filePath,
    const nx_http::StringType& mimeType)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const auto fileContents = f.readAll();
    return registerStaticProcessor(
        httpPath,
        std::move(fileContents),
        mimeType);
}

//-------------------------------------------------------------------------------------------------
// class RandomlyFailingHttpConnection

RandomlyFailingHttpConnection::RandomlyFailingHttpConnection(
    StreamConnectionHolder<RandomlyFailingHttpConnection>* socketServer,
    std::unique_ptr<AbstractCommunicatingSocket> sock)
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

void RandomlyFailingHttpConnection::onResponseSent(SystemError::ErrorCode sysErrorCode)
{
    //closeConnection(sysErrorCode);
}

//-------------------------------------------------------------------------------------------------
// class RandomlyFailingHttpServer

RandomlyFailingHttpServer::RandomlyFailingHttpServer(
    bool sslRequired,
    nx::network::NatTraversalSupport natTraversalSupport)
    :
    BaseType(sslRequired, natTraversalSupport)
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
