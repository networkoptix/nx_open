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

    m_thread.reset(new QThread());
    m_thread->setObjectName("QnMulticast::HttpServer");

    m_transport.reset(new Transport(localGuid, m_thread.get()));
    m_transport->setRequestCallback(std::bind(&HttpServer::at_gotRequest, this, _1, _2, _3));

    m_thread->start();
}

void HttpServer::at_gotRequest(const QUuid& requestId, const QUuid& clientId, const Request& request)
{
    int port = MSSettings::roSettings()->value("port", 7001).toInt();
    QString url(lit("http://%1:%2/%3").arg("127.0.0.1").arg(port).arg(request.url.toString()));

    nx_http::AsyncHttpClientPtr httpClient = nx_http::AsyncHttpClient::create();
    for (const auto& header: request.extraHttpHeaders)
        httpClient->addAdditionalHeader(header.first.toUtf8(), header.second.toUtf8());
    httpClient->addAdditionalHeader("Connection", "Close");

    connect(httpClient.get(), &nx_http::AsyncHttpClient::done, this, [requestId, clientId, this](nx_http::AsyncHttpClientPtr reply)
    {
        QnMulticast::Response response;
        response.serverId = qnCommon->moduleGUID().toQUuid();
        //response.messageBody = reply->fetchMessageBodyBuffer();
        //response.httpResult = reply->response()->statusLine.statusCode;
        //m_transport->addResponse(requestId, clientId, response);
        if (reply->response())
            m_transport->addResponse(requestId, clientId, reply->response()->toString());
    }, Qt::DirectConnection);

    connect(httpClient.get(), &nx_http::AsyncHttpClient::responseReceived, ec2::DummyHandler::instance(), [httpClient, requestId, clientId, this](nx_http::AsyncHttpClientPtr)  mutable 
    {
        httpClient->disconnect( nullptr, (const char*)nullptr );
        httpClient.reset();

        QnMulticast::Response response;
        response.serverId = qnCommon->moduleGUID().toQUuid();
        //response.messageBody = reply->fetchMessageBodyBuffer();
        //response.httpResult = reply->response()->statusLine.statusCode;
        //m_transport->addResponse(requestId, clientId, response);
        if (httpClient->response())
            m_transport->addResponse(requestId, clientId, httpClient->response()->toString());
    }, Qt::DirectConnection);



    bool result = false;
    if (request.method == "GET")
        result = httpClient->doGet(url);
    else if (request.method == "POST")
        result = httpClient->doPost(url, request.contentType, request.messageBody);
}


}
