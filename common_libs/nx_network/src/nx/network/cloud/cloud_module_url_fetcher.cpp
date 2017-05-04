#include "cloud_module_url_fetcher.h"

#include <QtCore/QBuffer>

#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/software_version.h>
#include <nx/utils/stree/resourcecontainer.h>
#include <nx/utils/stree/stree_manager.h>

#include "nx/network/app_info.h"
#include "cloud_modules_xml_sax_handler.h"

namespace nx {
namespace network {
namespace cloud {

static constexpr const char* const kCloudDbModuleName = "cdb";
static constexpr const char* const kConnectionMediatorModuleName = "hpm";
static constexpr const char* const kNotificationModuleName = "notification_module";
static constexpr std::chrono::seconds kHttpRequestTimeout = std::chrono::seconds(10);

//-------------------------------------------------------------------------------------------------
// class CloudInstanceSelectionAttributeNameset

CloudInstanceSelectionAttributeNameset::CloudInstanceSelectionAttributeNameset()
{
    registerResource(cloudInstanceName, "cloud.instance.name", QVariant::String);
    registerResource(vmsVersionMajor, "vms.version.major", QVariant::Int);
    registerResource(vmsVersionMinor, "vms.version.minor", QVariant::Int);
    registerResource(vmsVersionBugfix, "vms.version.bugfix", QVariant::Int);
    registerResource(vmsVersionBuild, "vms.version.build", QVariant::Int);
    registerResource(vmsVersionFull, "vms.version.full", QVariant::String);
    registerResource(vmsBeta, "vms.beta", QVariant::String);
    registerResource(vmsCustomization, "vms.customization", QVariant::String);
    registerResource(cdbUrl, kCloudDbModuleName, QVariant::String);
    registerResource(hpmUrl, kConnectionMediatorModuleName, QVariant::String);
    registerResource(notificationModuleUrl, kNotificationModuleName, QVariant::String);
}

//-------------------------------------------------------------------------------------------------
// class CloudModuleUrlFetcher

CloudModuleUrlFetcher::CloudModuleUrlFetcher(
    const QString& moduleName,
    std::unique_ptr<AbstractEndpointSelector> endpointSelector)
:
    m_moduleAttrName(m_nameset.findResourceByName(moduleName).id),
    m_endpointSelector(std::move(endpointSelector)),
    m_requestIsRunning(false),
    m_modulesXmlUrl(AppInfo::defaultCloudModulesXmlUrl())
{
    NX_ASSERT(
        m_moduleAttrName != nx::utils::stree::INVALID_RES_ID,
        Q_FUNC_INFO,
        lit("Given bad cloud module name %1").arg(moduleName));

    // Preparing compatibility data.
    m_moduleToDefaultUrlScheme.emplace(kCloudDbModuleName, "http");
    m_moduleToDefaultUrlScheme.emplace(kConnectionMediatorModuleName, "stun");
    m_moduleToDefaultUrlScheme.emplace(kNotificationModuleName, "http");
}

CloudModuleUrlFetcher::~CloudModuleUrlFetcher()
{
    stopWhileInAioThread();
}

void CloudModuleUrlFetcher::stopWhileInAioThread()
{
    //We do not need mutex here since no one uses object anymore
    //    and internal events are delivered in same aio thread.
    m_httpClient.reset();
    m_endpointSelector.reset();
}

void CloudModuleUrlFetcher::setModulesXmlUrl(QUrl url)
{
    m_modulesXmlUrl = std::move(url);
}

void CloudModuleUrlFetcher::setUrl(QUrl endpoint)
{
    QnMutexLocker lk(&m_mutex);
    m_url = std::move(endpoint);
}

void CloudModuleUrlFetcher::get(Handler handler)
{
    get(nx_http::AuthInfo(), std::move(handler));
}

void CloudModuleUrlFetcher::get(nx_http::AuthInfo auth, Handler handler)
{
    using namespace std::chrono;
    using namespace std::placeholders;

    //if requested endpoint is known, providing it to the output
    QnMutexLocker lk(&m_mutex);
    if (m_url)
    {
        auto result = m_url.get();
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
    m_httpClient->setAuth(auth);
    m_httpClient->bindToAioThread(getAioThread());

    for (const auto& header: m_additionalHttpHeadersForGetRequest)
        m_httpClient->addAdditionalHeader(header.first, header.second);

    m_httpClient->setSendTimeoutMs(
        duration_cast<milliseconds>(kHttpRequestTimeout).count());
    m_httpClient->setResponseReadTimeoutMs(
        duration_cast<milliseconds>(kHttpRequestTimeout).count());
    m_httpClient->setMessageBodyReadTimeoutMs(
        duration_cast<milliseconds>(kHttpRequestTimeout).count());

    m_requestIsRunning = true;
    m_httpClient->doGet(
        m_modulesXmlUrl,
        std::bind(&CloudModuleUrlFetcher::onHttpClientDone, this, _1));
}

void CloudModuleUrlFetcher::addAdditionalHttpHeaderForGetRequest(
    nx::String name, nx::String value)
{
    m_additionalHttpHeadersForGetRequest.emplace_back(name, value);
}

void CloudModuleUrlFetcher::onHttpClientDone(nx_http::AsyncHttpClientPtr client)
{
    QnMutexLocker lk(&m_mutex);

    m_httpClient.reset();

    if (!client->response())
    {
        return signalWaitingHandlers(
            &lk,
            nx_http::StatusCode::serviceUnavailable,
            QUrl());
    }

    if (client->response()->statusLine.statusCode != nx_http::StatusCode::ok)
    {
        return signalWaitingHandlers(
            &lk,
            static_cast<nx_http::StatusCode::Value>(
                client->response()->statusLine.statusCode),
            QUrl());
    }

    QByteArray xmlData = client->fetchMessageBodyBuffer();
    QBuffer xmlDataSource(&xmlData);
    std::unique_ptr<nx::utils::stree::AbstractNode> stree =
        nx::utils::stree::StreeManager::loadStree(&xmlDataSource, m_nameset);
    if (!stree)
    {
        return signalWaitingHandlers(
            &lk,
            nx_http::StatusCode::serviceUnavailable,
            QUrl());
    }

    //selecting endpoint
    QUrl moduleUrl;
    if (!findModuleUrl(*stree, m_moduleAttrName, &moduleUrl))
    {
        return signalWaitingHandlers(
            &lk,
            nx_http::StatusCode::notFound,
            QUrl());
    }

    saveFoundUrl(&lk, nx_http::StatusCode::ok, moduleUrl);
}

bool CloudModuleUrlFetcher::findModuleUrl(
    const nx::utils::stree::AbstractNode& treeRoot,
    const int moduleAttrName,
    QUrl* const moduleUrl)
{
    nx::utils::stree::ResourceContainer inputData;
    const nx::utils::SoftwareVersion productVersion(nx::utils::AppInfo::applicationVersion());
    inputData.put(
        CloudInstanceSelectionAttributeNameset::vmsVersionMajor,
        productVersion.major());
    inputData.put(
        CloudInstanceSelectionAttributeNameset::vmsVersionMinor,
        productVersion.minor());
    inputData.put(
        CloudInstanceSelectionAttributeNameset::vmsVersionBugfix,
        productVersion.bugfix());
    inputData.put(
        CloudInstanceSelectionAttributeNameset::vmsVersionBuild,
        productVersion.build());
    inputData.put(
        CloudInstanceSelectionAttributeNameset::vmsVersionFull,
        nx::utils::AppInfo::applicationVersion());
    inputData.put(
        CloudInstanceSelectionAttributeNameset::vmsBeta,
        nx::utils::AppInfo::beta() ? "true" : "false");
    inputData.put(
        CloudInstanceSelectionAttributeNameset::vmsCustomization,
        nx::utils::AppInfo::customizationName());

    nx::utils::stree::ResourceContainer outputData;
    treeRoot.get(nx::utils::stree::MultiSourceResourceReader(inputData, outputData), &outputData);

    QString foundEndpointStr;
    if (outputData.get(moduleAttrName, &foundEndpointStr))
    {
        *moduleUrl = buildUrl(foundEndpointStr);
        return true;
    }
    return false;
}

void CloudModuleUrlFetcher::signalWaitingHandlers(
    QnMutexLockerBase* const lk,
    nx_http::StatusCode::Value statusCode,
    const QUrl& endpoint)
{
    auto handlers = std::move(m_resolveHandlers);
    m_resolveHandlers = decltype(m_resolveHandlers)();
    m_requestIsRunning = false;
    lk->unlock();
    for (auto& handler : handlers)
        handler(statusCode, endpoint);
    lk->relock();
}

void CloudModuleUrlFetcher::saveFoundUrl(
    QnMutexLockerBase* const lk,
    nx_http::StatusCode::Value result,
    QUrl selectedEndpoint)
{
    if (result != nx_http::StatusCode::ok)
        return signalWaitingHandlers(lk, result, std::move(selectedEndpoint));

    NX_ASSERT(!m_url);
    m_url = selectedEndpoint;
    signalWaitingHandlers(lk, result, selectedEndpoint);
}

QUrl CloudModuleUrlFetcher::buildUrl(const QString& str)
{
    QUrl url(str);
    if (url.host().isEmpty())
    {
        // str could be host:port
        const SocketAddress endpoint(str);
        url = QUrl();
        url.setHost(endpoint.address.toString());
        if (endpoint.port > 0)
            url.setPort(endpoint.port);
    }

    if (url.scheme().isEmpty())
    {
        const auto it = m_moduleToDefaultUrlScheme.find(
            m_nameset.findResourceByID(m_moduleAttrName).name);
        if (it != m_moduleToDefaultUrlScheme.end())
            url.setScheme(it->second);
        else
            url.setScheme("http");

        if ((url.scheme() == "http") && (url.port() == nx_http::DEFAULT_HTTPS_PORT))
            url.setScheme("https");
    }

    return url;
}

//-------------------------------------------------------------------------------------------------

CloudModuleUrlFetcher::ScopedOperation::ScopedOperation(
    CloudModuleUrlFetcher* const fetcher)
:
    m_fetcher(fetcher)
{
}

CloudModuleUrlFetcher::ScopedOperation::~ScopedOperation()
{
}

void CloudModuleUrlFetcher::ScopedOperation::get(Handler handler)
{
    get(nx_http::AuthInfo(), std::move(handler));
}

void CloudModuleUrlFetcher::ScopedOperation::get(nx_http::AuthInfo auth, Handler handler)
{
    auto sharedGuard = m_guard.sharedGuard();
    m_fetcher->get(
        auth,
        [sharedGuard = std::move(sharedGuard), handler = std::move(handler)](
            nx_http::StatusCode::Value statusCode, QUrl result) mutable
        {
            if (auto lock = sharedGuard->lock())
                handler(statusCode, result);
        });
}

//-------------------------------------------------------------------------------------------------
// class CloudDbUrlFetcher

CloudDbUrlFetcher::CloudDbUrlFetcher(
    std::unique_ptr<AbstractEndpointSelector> endpointSelector)
    :
    CloudModuleUrlFetcher(kCloudDbModuleName, std::move(endpointSelector))
{
}

//-------------------------------------------------------------------------------------------------
// class ConnectionMediatorUrlFetcher

ConnectionMediatorUrlFetcher::ConnectionMediatorUrlFetcher(
    std::unique_ptr<AbstractEndpointSelector> endpointSelector)
    :
    CloudModuleUrlFetcher(kConnectionMediatorModuleName, std::move(endpointSelector))
{
}

} // namespace cloud
} // namespace network
} // namespace nx
