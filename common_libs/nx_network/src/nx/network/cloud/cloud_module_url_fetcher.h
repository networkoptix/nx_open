#pragma once

#include <functional>
#include <map>
#include <list>
#include <vector>

#include <boost/optional.hpp>

#include <QtCore/QString>
#include <QtCore/QUrl>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/move_only_func.h>

#include <nx/utils/stree/node.h>
#include <nx/utils/stree/resourcenameset.h>

#include "endpoint_selector.h"

namespace nx {
namespace network {
namespace cloud {

class CloudInstanceSelectionAttributeNameset:
    public nx::utils::stree::ResourceNameSet
{
public:
    enum AttributeType
    {
        cloudInstanceName = 1,
        vmsVersionMajor,
        vmsVersionMinor,
        vmsVersionBugfix,
        vmsVersionBuild,
        vmsVersionFull,
        vmsBeta,
        vmsCustomization,
        cdbUrl,
        hpmUrl,
        notificationModuleUrl
    };

    CloudInstanceSelectionAttributeNameset();
};

/**
 * Looks up online API url of a specified cloud module. 
 */
class NX_NETWORK_API CloudModuleUrlFetcher:
    public aio::BasicPollable
{
public:
    typedef nx::utils::MoveOnlyFunc<void(nx_http::StatusCode::Value, QUrl)> Handler;

    /**
     * Helper class to be used if CloudModuleUrlFetcher user can die before
     *     CloudModuleUrlFetcher instance.
     */
    class NX_NETWORK_API ScopedOperation
    {
    public:
        ScopedOperation(CloudModuleUrlFetcher* fetcher);
        ~ScopedOperation();

        void get(nx_http::AuthInfo auth, Handler handler);
        void get(Handler handler);

    private:
        CloudModuleUrlFetcher* const m_fetcher;
        nx::utils::AsyncOperationGuard m_guard;
    };

    CloudModuleUrlFetcher(
        const QString& moduleName,
        std::unique_ptr<AbstractEndpointSelector> endpointSelector);
    virtual ~CloudModuleUrlFetcher();

    virtual void stopWhileInAioThread() override;

    /**
     * Default value taken from application setup.
     */
    void setModulesXmlUrl(QUrl url);

    /**
     * Specify url explicitly.
     */
    void setUrl(QUrl url);
    /**
     * Retrieves endpoint if unknown. 
     * If endpoint is known, then calls handler directly from this method.
     */
    void get(nx_http::AuthInfo auth, Handler handler);
    void get(Handler handler);

    void addAdditionalHttpHeaderForGetRequest(
        nx::String name, nx::String value);

private:
    mutable QnMutex m_mutex;
    boost::optional<QUrl> m_url;
    nx_http::AsyncHttpClientPtr m_httpClient;
    const CloudInstanceSelectionAttributeNameset m_nameset;
    const int m_moduleAttrName;
    std::vector<Handler> m_resolveHandlers;
    std::unique_ptr<AbstractEndpointSelector> m_endpointSelector;
    bool m_requestIsRunning;
    QUrl m_modulesXmlUrl;
    std::map<QString, QString> m_moduleToDefaultUrlScheme;
    std::list<std::pair<nx::String, nx::String>> m_additionalHttpHeadersForGetRequest;

    void onHttpClientDone(nx_http::AsyncHttpClientPtr client);
    bool findModuleUrl(
        const nx::utils::stree::AbstractNode& treeRoot,
        const int moduleAttrName,
        QUrl* const moduleEndpoint);
    void signalWaitingHandlers(
        QnMutexLockerBase* const lk,
        nx_http::StatusCode::Value statusCode,
        const QUrl& endpoint);
    void saveFoundUrl(
        QnMutexLockerBase* const lk,
        nx_http::StatusCode::Value result,
        QUrl selectedEndpoint);
    QUrl buildUrl(const QString& str);
};

class NX_NETWORK_API CloudDbUrlFetcher:
    public CloudModuleUrlFetcher
{
public:
    CloudDbUrlFetcher(
        std::unique_ptr<AbstractEndpointSelector> endpointSelector);
};

class NX_NETWORK_API ConnectionMediatorUrlFetcher:
    public CloudModuleUrlFetcher
{
public:
    ConnectionMediatorUrlFetcher(
        std::unique_ptr<AbstractEndpointSelector> endpointSelector);
};

} // namespace cloud
} // namespace network
} // namespace nx
