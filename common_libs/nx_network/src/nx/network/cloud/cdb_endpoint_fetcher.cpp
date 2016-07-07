/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include "cdb_endpoint_fetcher.h"

#include <QtCore/QBuffer>

#include <nx/network/app_info.h>

#include <nx/utils/log/log.h>
#include <utils/common/guard.h>

#include "cloud_modules_xml_sax_handler.h"


namespace nx {
namespace network {
namespace cloud {

CloudModuleEndPointFetcher::CloudModuleEndPointFetcher(
    QString moduleName,
    std::unique_ptr<AbstractEndpointSelector> endpointSelector)
:
    m_moduleName(std::move(moduleName)),
    m_endpointSelector(std::move(endpointSelector)),
    m_requestIsRunning(false)
{
}

CloudModuleEndPointFetcher::~CloudModuleEndPointFetcher()
{
    stopWhileInAioThread();
}

void CloudModuleEndPointFetcher::stopWhileInAioThread()
{
    //we do not need mutex here since noone uses object anymore 
    //    and internal events are delivered in same aio thread
    m_httpClient.reset();
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

    //if async resolve is already started, should wait for its completion
    m_resolveHandlers.emplace_back(std::move(handler));

    if (m_requestIsRunning)
        return;

    NX_ASSERT(!m_httpClient);
    //if requested url is unknown, fetching description xml
    m_httpClient = nx_http::AsyncHttpClient::create();
    m_httpClient->bindToAioThread(getAioThread());
    QObject::connect(
        m_httpClient.get(), &nx_http::AsyncHttpClient::done,
        m_httpClient.get(), [this](nx_http::AsyncHttpClientPtr client){
            onHttpClientDone(std::move(client));
        },
        Qt::DirectConnection);
    m_requestIsRunning = true;
    m_httpClient->doGet(QUrl(nx::network::AppInfo::cloudModulesXmlUrl()));
}

void CloudModuleEndPointFetcher::onHttpClientDone(nx_http::AsyncHttpClientPtr client)
{
    QnMutexLocker lk(&m_mutex);

    m_httpClient.reset();

    if (!client->response())
        return signalWaitingHandlers(
            &lk,
            nx_http::StatusCode::serviceUnavailable,
            SocketAddress());

    if (client->response()->statusLine.statusCode != nx_http::StatusCode::ok)
        return signalWaitingHandlers(
            &lk,
            static_cast<nx_http::StatusCode::Value>(
                client->response()->statusLine.statusCode),
            SocketAddress());

    std::vector<SocketAddress> endpoints;
    if (!parseCloudXml(client->fetchMessageBodyBuffer(), &endpoints) ||
        endpoints.empty())
    {
        return signalWaitingHandlers(
            &lk,
            nx_http::StatusCode::serviceUnavailable,
            SocketAddress());
    }

    using namespace std::placeholders;
    //selecting "best" endpoint
    m_endpointSelector->selectBestEndpont(
        m_moduleName,
        std::move(endpoints),
        [this](
            nx_http::StatusCode::Value result,
            SocketAddress selectedEndpoint)
        {
            post(std::bind(
                &CloudModuleEndPointFetcher::endpointSelected, this,
                result, std::move(selectedEndpoint)));
        });
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
               .arg(nx::network::AppInfo::cloudModulesXmlUrl()).arg(xmlHandler.errorString()),
               cl_logERROR);
        return false;
    }

    *endpoints = xmlHandler.moduleUrls(m_moduleName);
    return true;
}

void CloudModuleEndPointFetcher::signalWaitingHandlers(
    QnMutexLockerBase* const lk,
    nx_http::StatusCode::Value statusCode,
    const SocketAddress& endpoint)
{
    auto handlers = std::move(m_resolveHandlers);
    m_resolveHandlers = decltype(m_resolveHandlers)();
    m_requestIsRunning = false;
    lk->unlock();
    for (auto& handler : handlers)
        handler(statusCode, endpoint);
    lk->relock();
}

void CloudModuleEndPointFetcher::endpointSelected(
    nx_http::StatusCode::Value result,
    SocketAddress selectedEndpoint)
{
    QnMutexLocker lk(&m_mutex);

    if (result != nx_http::StatusCode::ok)
        return signalWaitingHandlers(&lk, result, std::move(selectedEndpoint));

    NX_ASSERT(!m_endpoint);
    m_endpoint = selectedEndpoint;
    signalWaitingHandlers(&lk, result, selectedEndpoint);
}

} // namespace cloud
} // namespace network
} // namespace nx
