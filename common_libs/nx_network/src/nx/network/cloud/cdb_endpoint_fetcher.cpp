/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include "cdb_endpoint_fetcher.h"

#include <QtCore/QBuffer>

#include <nx/utils/log/log.h>

#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <plugins/videodecoder/stree/stree_manager.h>
#include <utils/common/app_info.h>
#include <utils/common/guard.h>
#include <utils/common/software_version.h>

#include "cloud_modules_xml_sax_handler.h"


namespace nx {
namespace network {
namespace cloud {

CloudModuleEndPointFetcher::CloudModuleEndPointFetcher(
    const QString& moduleName,
    std::unique_ptr<AbstractEndpointSelector> endpointSelector)
:
    m_moduleAttrName(m_nameset.findResourceByName(moduleName).id),
    m_endpointSelector(std::move(endpointSelector)),
    m_requestIsRunning(false)
{
    NX_ASSERT(
        m_moduleAttrName != stree::INVALID_RES_ID,
        Q_FUNC_INFO,
        lit("Given bad cloud module name %1").arg(moduleName));
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
    nx::utils::MoveOnlyFunc<void(nx_http::StatusCode::Value, SocketAddress)> handler)
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
    m_httpClient->doGet(QUrl(QnAppInfo::defaultCloudModulesXmlUrl()));
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

    QByteArray xmlData = client->fetchMessageBodyBuffer();
    QBuffer xmlDataSource(&xmlData);
    std::unique_ptr<stree::AbstractNode> stree =
        stree::StreeManager::loadStree(&xmlDataSource, m_nameset);
    if (!stree)
        return signalWaitingHandlers(
            &lk,
            nx_http::StatusCode::serviceUnavailable,
            SocketAddress());

    //selecting endpoint
    SocketAddress moduleEndpoint;
    if (!findModuleEndpoint(*stree, m_moduleAttrName, &moduleEndpoint))
        return signalWaitingHandlers(
            &lk,
            nx_http::StatusCode::notFound,
            SocketAddress());

    endpointSelected(&lk, nx_http::StatusCode::ok, moduleEndpoint);
}

bool CloudModuleEndPointFetcher::findModuleEndpoint(
    const stree::AbstractNode& treeRoot,
    const int moduleAttrName,
    SocketAddress* const moduleEndpoint)
{
    stree::ResourceContainer inputData;
    const QnSoftwareVersion productVersion(QnAppInfo::applicationVersion());
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
        QnAppInfo::applicationVersion());
    inputData.put(
        CloudInstanceSelectionAttributeNameset::vmsBeta,
        QnAppInfo::beta() ? "true" : "false");
    inputData.put(
        CloudInstanceSelectionAttributeNameset::vmsCustomization,
        QnAppInfo::customizationName());

    stree::ResourceContainer outputData;
    treeRoot.get(stree::MultiSourceResourceReader(inputData, outputData), &outputData);

    QString foundEndpointStr;
    if (outputData.get(moduleAttrName, &foundEndpointStr))
    {
        *moduleEndpoint = SocketAddress(foundEndpointStr);
        return true;
    }
    return false;
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
    QnMutexLockerBase* const lk,
    nx_http::StatusCode::Value result,
    SocketAddress selectedEndpoint)
{
    if (result != nx_http::StatusCode::ok)
        return signalWaitingHandlers(lk, result, std::move(selectedEndpoint));

    NX_ASSERT(!m_endpoint);
    m_endpoint = selectedEndpoint;
    signalWaitingHandlers(lk, result, selectedEndpoint);
}


CloudModuleEndPointFetcher::ScopedOperation::ScopedOperation(
    CloudModuleEndPointFetcher* const fetcher)
:
    m_fetcher(fetcher)
{
}

CloudModuleEndPointFetcher::ScopedOperation::~ScopedOperation()
{
}

void CloudModuleEndPointFetcher::ScopedOperation::get(
    nx::utils::MoveOnlyFunc<void(nx_http::StatusCode::Value, SocketAddress)> handler)
{
    auto sharedGuard = m_guard.sharedGuard();
    m_fetcher->get(
        [sharedGuard = std::move(sharedGuard), handler = std::move(handler)](
            nx_http::StatusCode::Value statusCode, SocketAddress result) mutable
        {
            if (auto lock = sharedGuard->lock())
                handler(statusCode, result);
        });
}


////////////////////////////////////////////////////////////
//// class CloudInstanceSelectionAttributeNameset
////////////////////////////////////////////////////////////

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
    registerResource(cdbEndpoint, "cdb", QVariant::String);
    registerResource(hpmEndpoint, "hpm", QVariant::String);
    registerResource(notificationModuleEndpoint, "notification_module", QVariant::String);
}

} // namespace cloud
} // namespace network
} // namespace nx
