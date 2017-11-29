#include "multicast_http_server.h"
#include <QThread>
#include "media_server/settings.h"
#include <nx/network/deprecated/simple_http_client.h>
#include "common/common_module.h"
#include "nx_ec/dummy_handler.h"
#include "network/tcp_listener.h"


namespace QnMulticast
{

HttpServer::HttpServer(const QUuid& localGuid, QnTcpListener* tcpListener):
    m_tcpListener(tcpListener)
{
    using namespace std::placeholders;

    m_transport.reset(new Transport(localGuid));
    m_transport->setRequestCallback(std::bind(&HttpServer::at_gotRequest, this, _1, _2, _3));
}

void HttpServer::at_gotRequest(const QUuid& requestId, const QUuid& clientId, const Request& request)
{
    QString urlStr = request.url.toString();
    while (urlStr.startsWith(lit("/")))
        urlStr = urlStr.mid(1);
    QString url(lit("http://%1:%2/%3").arg("127.0.0.1").arg(m_tcpListener->getPort()).arg(urlStr));

    nx_http::AsyncHttpClientPtr httpClient = nx_http::AsyncHttpClient::create();
    for (const auto& header: request.headers) {
        if (header.first == QLatin1String("User-Agent"))
            httpClient->setUserAgent(header.second);
        else
            httpClient->addAdditionalHeader(header.first.toUtf8(), header.second.toUtf8());
    }
    httpClient->addAdditionalHeader("Connection", "Close");

    connect(httpClient.get(), &nx_http::AsyncHttpClient::done, this, [httpClient, requestId, clientId, this](nx_http::AsyncHttpClientPtr)  mutable 
    {
        if (httpClient->response())
            m_transport->addResponse(requestId, clientId, httpClient->response()->toString() + httpClient->fetchMessageBodyBuffer());
        httpClient->disconnect( nullptr, (const char*)nullptr );
        httpClient.reset();
    }, Qt::DirectConnection);

    if (request.method == nx_http::Method::get)
    {
        httpClient->doGet(url);
    }
    else if (request.method == nx_http::Method::post)
    {
        httpClient->doPost(url, request.contentType, request.messageBody);
    }
    else
    {
        qWarning() << "Got unknown HTTP method" << request.method << "over HTTP multicast transport";
        disconnect(httpClient.get(), nullptr, this, nullptr);
    }
}

}
