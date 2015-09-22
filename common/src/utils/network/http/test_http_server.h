/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#ifndef TEST_HTTP_SERVER_H
#define TEST_HTTP_SERVER_H

#include <memory>

#include <QtCore/QString>

#include <utils/network/connection_server/multi_address_server.h>
#include <utils/network/http/server/http_stream_socket_server.h>
#include <utils/network/http/server/http_message_dispatcher.h>


class TestHttpServer
{
public:
    TestHttpServer();
    ~TestHttpServer();

    bool bindAndListen();
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

    bool registerStaticProcessor( const QString& path, QByteArray response );

private:
    nx_http::MessageDispatcher m_httpMessageDispatcher;
    std::unique_ptr<nx_http::HttpStreamSocketServer> m_httpServer;
};

#endif  //TEST_HTTP_SERVER_H
