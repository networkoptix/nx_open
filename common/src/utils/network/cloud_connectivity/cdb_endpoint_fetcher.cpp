/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include "cdb_endpoint_fetcher.h"

#include <QtCore/QBuffer>

#include <common/common_globals.h>
#include <utils/common/log.h>

#include "cloud_modules_xml_sax_handler.h"
#include "version.h"

namespace nx {
namespace cc {

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

void CloudModuleEndPointFetcher::setEndpoint(SocketAddress endpoint)
{
    QnMutexLocker lk(&m_mutex);
    m_endpoint = std::move(endpoint);
}

//!Retrieves endpoint if unknown. If endpoint is known, then calls \a handler directly from this method
void CloudModuleEndPointFetcher::get(
    std::function<void(nx_http::StatusCode::Value, SocketAddress)> handler)
{
    //TODO #ak if async resolve is already started, should wait for its completion

    //if requested endpoint is known, providing it to the output
    QnMutexLocker lk(&m_mutex);
    if (m_endpoint)
    {
        auto result = m_endpoint.get();
        lk.unlock();
        handler(nx_http::StatusCode::ok, std::move(result));
        return;
    }

    //TODO #ak if requested url is unknown, fetching description xml
    m_httpClient = nx_http::AsyncHttpClient::create();
    QObject::connect(
        m_httpClient.get(), &nx_http::AsyncHttpClient::done,
        m_httpClient.get(), [this, handler](nx_http::AsyncHttpClientPtr client){
            onHttpClientDone(std::move(client), std::move(handler)); },
        Qt::DirectConnection);
    if (!m_httpClient->doGet(QUrl(lit(QN_CLOUD_MODULES_XML))))
    {
        m_httpClient.reset();
        return handler(nx_http::StatusCode::serviceUnavailable, SocketAddress());
    }
}

void CloudModuleEndPointFetcher::onHttpClientDone(
    nx_http::AsyncHttpClientPtr client,
    std::function<void(nx_http::StatusCode::Value, SocketAddress)> handler)
{
    auto localHandler = [this, handler](nx_http::StatusCode::Value resCode,
                                        SocketAddress endpoint)
    {
        handler(resCode, std::move(endpoint));
        QnMutexLocker lk(&m_mutex);
        m_httpClient.reset();
    };

    if (!client->response())
        return localHandler(nx_http::StatusCode::serviceUnavailable,
                            SocketAddress());

    if (client->response()->statusLine.statusCode != nx_http::StatusCode::ok)
        return localHandler(
            static_cast< nx_http::StatusCode::Value >(
                client->response()->statusLine.statusCode ),
            SocketAddress());

    parseCloudXml(client->fetchMessageBodyBuffer());

    //TODO #ak selecting endpoint we're able to connect to

    QnMutexLocker lk(&m_mutex);
    auto endpoint = m_endpoint;
    lk.unlock();

    if (!endpoint)
        return localHandler(nx_http::StatusCode::serviceUnavailable,
                            SocketAddress());

    localHandler(nx_http::StatusCode::ok, m_endpoint.get());
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
        NX_LOG(lit("Failed to parse cloud_modules.xml from %1. %2")
               .arg(lit(QN_CLOUD_MODULES_XML)).arg(xmlHandler.errorString()),
               cl_logERROR);
        return;
    }

    const auto endpoints = xmlHandler.moduleUrls(m_moduleName);
    if (!endpoints.empty())
    {
        QnMutexLocker lk(&m_mutex);
        if (!m_endpoint)
            m_endpoint = std::move(endpoints.front());
    }
}

}   //cc
}   //nx
