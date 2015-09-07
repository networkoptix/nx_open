#include "multicast_http_server.h"
#include <QThread>
#include "media_server/settings.h"
#include "utils/network/simple_http_client.h"
#include "common/common_module.h"
#include "nx_ec/dummy_handler.h"

namespace QnMulticast
{

HttpServer::HttpServer(const QUuid& localGuid)
{
    using namespace std::placeholders;

    m_transport.reset(new Transport(localGuid));
    m_transport->setRequestCallback(std::bind(&HttpServer::at_gotRequest, this, _1, _2, _3));
}

void HttpServer::at_gotRequest(const QUuid& requestId, const QUuid& clientId, const Request& request)
{
    int port = MSSettings::roSettings()->value("port", 7001).toInt();
    QString url(lit("http://%1:%2/%3").arg("127.0.0.1").arg(port).arg(request.url.toString()));

    nx_http::AsyncHttpClientPtr httpClient = nx_http::AsyncHttpClient::create();
    for (const auto& header: request.extraHttpHeaders)
        httpClient->addAdditionalHeader(header.first.toUtf8(), header.second.toUtf8());
    httpClient->addAdditionalHeader("Connection", "Close");

    connect(httpClient.get(), &nx_http::AsyncHttpClient::done, this, [httpClient, requestId, clientId, this](nx_http::AsyncHttpClientPtr)  mutable 
    {
        if (httpClient->response())
            m_transport->addResponse(requestId, clientId, httpClient->response()->toString() + httpClient->fetchMessageBodyBuffer());
        httpClient->disconnect( nullptr, (const char*)nullptr );
        httpClient.reset();
    }, Qt::DirectConnection);

    bool result = false;
    if (request.method == "GET")
        result = httpClient->doGet(url);
    else if (request.method == "POST")
        result = httpClient->doPost(url, request.contentType, request.messageBody);
    if (!result)
        disconnect( httpClient.get(), nullptr, this, nullptr );
}


}
