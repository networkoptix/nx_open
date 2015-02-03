/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#ifndef TEST_HTTP_SERVER_H
#define TEST_HTTP_SERVER_H

#include <memory>

#include <utils/network/connection_server/multi_address_server.h>
#include <utils/network/http/server/http_stream_socket_server.h>
#include <utils/network/http/server/http_message_dispatcher.h>


class TestHttpServer
{
public:
    TestHttpServer();
    ~TestHttpServer();

    SocketAddress serverAddress() const;

private:
    std::unique_ptr<nx_http::HttpStreamSocketServer> m_httpServer;
    nx_http::MessageDispatcher m_httpMessageDispatcher;
};

#endif  //TEST_HTTP_SERVER_H
