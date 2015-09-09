/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include "cdb_endpoint_fetcher.h"

#include <QtCore/QBuffer>

#include <common/common_globals.h>
#include <utils/common/log.h>

#include "cloud_modules_xml_sax_handler.h"
#include "data/types.h"
#include "version.h"


namespace nx {
namespace cdb {
namespace cl {

CloudModuleEndPointFetcher::CloudModuleEndPointFetcher(QString moduleName)
:
    m_moduleName(std::move(moduleName))
{
}

CloudModuleEndPointFetcher::~CloudModuleEndPointFetcher()
{
    QnMutexLocker lk(&m_mutex);
    auto httpClientLocal = std::move(m_httpClient);
    lk.unlock();
    if (httpClientLocal)
        httpClientLocal->terminate();
}

//!Retrieves endpoint if unknown. If endpoint is known, then calls \a handler directly from this method
void CloudModuleEndPointFetcher::get(
    QString username,
    QString password,
    std::function<void(api::ResultCode, SocketAddress)> handler)
{
    //if requested endpoint is known, providing it to the output
    QnMutexLocker lk(&m_mutex);
    if (m_endpoint)
    {
        auto result = m_endpoint.get();
        lk.unlock();
        handler(api::ResultCode::ok, std::move(result));
        return;
    }

    //TODO #ak if requested url is unknown, fetching description xml
    m_httpClient = nx_http::AsyncHttpClient::create();
    QObject::connect(
        m_httpClient.get(), &nx_http::AsyncHttpClient::done,
        m_httpClient.get(), [this, handler](nx_http::AsyncHttpClientPtr client){
            onHttpClientDone(std::move(client), std::move(handler)); },
        Qt::DirectConnection);
    if (!m_httpClient->doGet(QUrl(QN_CLOUD_MODULES_XML)))
    {
        m_httpClient.reset();
        return handler(api::ResultCode::networkError, SocketAddress());
    }
}

void CloudModuleEndPointFetcher::onHttpClientDone(
    nx_http::AsyncHttpClientPtr client,
    std::function<void(api::ResultCode, SocketAddress)> handler)
{
    auto localHandler = [this, handler](api::ResultCode resCode, SocketAddress endpoint){
        handler(resCode, std::move(endpoint));
        QnMutexLocker lk(&m_mutex);
        m_httpClient.reset();
    };

    if (!client->response())
        return localHandler(api::ResultCode::networkError, SocketAddress());

    if (client->response()->statusLine.statusCode != nx_http::StatusCode::ok)
        return localHandler(
            api::httpStatusCodeToResultCode(static_cast<nx_http::StatusCode::Value>(
                client->response()->statusLine.statusCode)),
            SocketAddress());

    parseCloudXml(client->fetchMessageBodyBuffer());

    QnMutexLocker lk(&m_mutex);
    auto endpoint = m_endpoint;
    lk.unlock();

    if (!endpoint)
        return localHandler(api::ResultCode::networkError, SocketAddress());
    
    localHandler(api::ResultCode::ok, m_endpoint.get());
}

void CloudModuleEndPointFetcher::parseCloudXml(QByteArray xmlData)
{
    CloudModulesXmlHandler xmlHandler;

    QXmlSimpleReader reader;
    reader.setContentHandler(&xmlHandler);
    reader.setErrorHandler(&xmlHandler);

    QBuffer xmlBuffer(&xmlData);
    QXmlInputSource input(&xmlBuffer);
    if (!reader.parse(&input))
    {
        NX_LOG(lit("Failed to parse cloud_modules.xml from %1. %2").
            arg(QN_CLOUD_MODULES_XML).arg(xmlHandler.errorString()), cl_logERROR);
        return;
    }

    const auto endpoints = xmlHandler.moduleUrls(m_moduleName);
    if (!endpoints.empty())
    {
        QnMutexLocker lk(&m_mutex);
        m_endpoint = std::move(endpoints.front());
    }
}


}   //cl
}   //cdb
}   //nx
