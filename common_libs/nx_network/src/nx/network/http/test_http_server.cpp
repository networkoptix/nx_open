/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include "test_http_server.h"

#include <QtCore/QFile>

#include <nx/network/http/buffer_source.h>


TestHttpServer::TestHttpServer()
{
    m_httpServer.reset(
        new nx_http::HttpStreamSocketServer(
            nullptr,
            &m_httpMessageDispatcher,
            false,
            SocketFactory::NatTraversalType::nttDisabled ) );
}

TestHttpServer::~TestHttpServer()
{
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
        const nx_http::HttpServerConnection& /*connection*/,
        stree::ResourceContainer /*authInfo*/,
        const nx_http::Request& /*request*/,
        nx_http::Response* const /*response*/,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )> completionHandler )
    {
        completionHandler(
            nx_http::StatusCode::ok,
            std::make_unique< nx_http::BufferSource >( m_mimeType, m_response ) );
    }

private:
    const nx_http::StringType m_mimeType;
    const QByteArray m_response;
};

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
