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

CloudModuleEndPointFetcher::CloudModuleEndPointFetcher(
    QString moduleName,
    std::unique_ptr<AbstractEndpointSelector> endpointSelector)
:
    m_moduleName(std::move(moduleName)),
    m_endpointSelector(std::move(endpointSelector))
{
}

CloudModuleEndPointFetcher::~CloudModuleEndPointFetcher()
{
    QnMutexLocker lk(&m_mutex);
    auto httpClientLocal = std::move(m_httpClient);
    lk.unlock();
    if (httpClientLocal)
        httpClientLocal->terminate();
    //assuming m_endpointSelector will wait for completion handler to return
    m_endpointSelector.reset();
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
    //if requested endpoint is known, providing it to the output
    QnMutexLocker lk(&m_mutex);
    if (m_endpoint)
    {
        auto result = m_endpoint.get();
        lk.unlock();
        handler(nx_http::StatusCode::ok, std::move(result));
        return;
    }

    //if requested url is unknown, fetching description xml
    m_httpClient = nx_http::AsyncHttpClient::create();
    QObject::connect(
        m_httpClient.get(), &nx_http::AsyncHttpClient::done,
        m_httpClient.get(), [this](nx_http::AsyncHttpClientPtr client){
            onHttpClientDone(std::move(client));
        },
        Qt::DirectConnection);
    m_httpClient->doGet(QUrl(lit(QN_CLOUD_MODULES_XML)));

    //if async resolve is already started, should wait for its completion
    m_resolveHandlers.emplace_back(std::move(handler));
}

void CloudModuleEndPointFetcher::onHttpClientDone(nx_http::AsyncHttpClientPtr client)
{
    if (!client->response())
        return signalWaitingHandlers(
            nx_http::StatusCode::serviceUnavailable,
            SocketAddress());

    if (client->response()->statusLine.statusCode != nx_http::StatusCode::ok)
        return signalWaitingHandlers(
            static_cast<nx_http::StatusCode::Value>(
                client->response()->statusLine.statusCode),
            SocketAddress());

    std::vector<SocketAddress> endpoints;
    if (!parseCloudXml(client->fetchMessageBodyBuffer(), &endpoints) ||
        endpoints.empty())
    {
        return signalWaitingHandlers(
            nx_http::StatusCode::serviceUnavailable,
            SocketAddress());
    }

    using namespace std::placeholders;
    //selecting "best" endpoint
    m_endpointSelector->selectBestEndpont(
        m_moduleName,
        std::move(endpoints),
        std::bind(&CloudModuleEndPointFetcher::endpointSelected, this, _1, _2));

    QnMutexLocker lk(&m_mutex);
    m_httpClient.reset();
}

bool CloudModuleEndPointFetcher::parseCloudXml(
    QByteArray xmlData,
    std::vector<SocketAddress>* const endpoints)
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
        return false;
    }

    *endpoints = xmlHandler.moduleUrls(m_moduleName);
    return true;
}

void CloudModuleEndPointFetcher::signalWaitingHandlers(
    nx_http::StatusCode::Value statusCode,
    const SocketAddress& endpoint)
{
    QnMutexLocker lk(&m_mutex);
    auto handlers = std::move(m_resolveHandlers);
    lk.unlock();
    for (auto& handler : handlers)
        handler(statusCode, endpoint);
    lk.relock();
    m_httpClient.reset();
}

void CloudModuleEndPointFetcher::endpointSelected(
    nx_http::StatusCode::Value result,
    SocketAddress selectedEndpoint)
{
    if (result != nx_http::StatusCode::ok)
        return signalWaitingHandlers(result, std::move(selectedEndpoint));

    QnMutexLocker lk(&m_mutex);
    Q_ASSERT(!m_endpoint);
    m_endpoint = selectedEndpoint;
    lk.unlock();
    signalWaitingHandlers(result, selectedEndpoint);
}

}   //cc
}   //nx
