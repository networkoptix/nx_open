#include "multicast_http_server.h"
#include <QThread>
#include "media_server/settings.h"
#include <nx/network/deprecated/simple_http_client.h>
#include "common/common_module.h"
#include "nx_ec/dummy_handler.h"
#include "network/tcp_listener.h"
#include <nx/network/http/buffer_source.h>
#include <nx/utils/log/log_main.h>

namespace QnMulticast
{

HttpServer::HttpServer(const QUuid& localGuid, QnTcpListener* tcpListener):
    m_tcpListener(tcpListener)
{
    using namespace std::placeholders;

    m_transport.reset(new Transport(localGuid));
    m_transport->setRequestCallback(std::bind(&HttpServer::at_gotRequest, this, _1, _2, _3));
}

HttpServer::~HttpServer()
{
    decltype (m_requests) requests;
    {
        QnMutexLocker lock(&m_mutex);
        std::swap(requests, m_requests);
    }

    for (auto& request: requests)
        request.second->pleaseStopSync();
}

void HttpServer::at_gotRequest(const QUuid& requestId, const QUuid& clientId, const Request& request)
{
    QnMutexLocker lock(&m_mutex);

    if (m_requests.find(requestId) != m_requests.end())
    {
        NX_ASSERT(0, lm("Request with Id %1 is already running.").arg(requestId));
        return;
    }

    QString urlStr = request.url.toString();
    while (urlStr.startsWith(lit("/")))
        urlStr = urlStr.mid(1);
    QString url(lit("http://%1:%2/%3").arg("127.0.0.1").arg(m_tcpListener->getPort()).arg(urlStr));

    auto httpClient = std::make_unique<nx::network::http::AsyncClient>();

    for (const auto& header: request.headers)
    {
        if (header.first == QLatin1String("User-Agent"))
            httpClient->setUserAgent(header.second);
        else
            httpClient->addAdditionalHeader(header.first.toUtf8(), header.second.toUtf8());
    }
    httpClient->addAdditionalHeader("Connection", "Close");

    httpClient->setOnDone(
        [requestId, clientId, this]()  mutable
        {
            QnMutexLocker lock(&m_mutex);
            auto itr = m_requests.find(requestId);
            if (itr == m_requests.end())
                return;
            auto httpClient = itr->second.get();
            if (httpClient->response())
                m_transport->addResponse(requestId, clientId, httpClient->response()->toString() + httpClient->fetchMessageBodyBuffer());
            m_requests.erase(itr);
        });

    if (request.method == nx::network::http::Method::get)
    {
        httpClient->doGet(url);
    }
    else if (request.method == nx::network::http::Method::post)
    {
        httpClient->setRequestBody(
            std::make_unique<nx::network::http::BufferSource>(
                request.contentType,
                request.messageBody));
        httpClient->doPost(url);
    }
    else
    {
        NX_WARNING(this, lm("Got unknown HTTP method %1 over HTTP multicast transport").arg(request.method));
        return;
    }

    m_requests.emplace(requestId, std::move(httpClient));
}

}
