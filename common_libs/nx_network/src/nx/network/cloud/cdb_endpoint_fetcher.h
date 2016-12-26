#pragma once

#include <functional>
#include <map>
#include <vector>

#include <boost/optional.hpp>

#include <QtCore/QString>
#include <QtCore/QUrl>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/move_only_func.h>

#include <plugins/videodecoder/stree/node.h>
#include <plugins/videodecoder/stree/resourcenameset.h>

#include "endpoint_selector.h"

namespace nx {
namespace network {
namespace cloud {

class CloudInstanceSelectionAttributeNameset:
    public stree::ResourceNameSet
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
        cdbEndpoint,
        hpmEndpoint,
        notificationModuleEndpoint
    };

    CloudInstanceSelectionAttributeNameset();
};

//!Retrieves url to the specified cloud module
class NX_NETWORK_API CloudModuleEndPointFetcher:
    public aio::BasicPollable
{
public:
    typedef nx::utils::MoveOnlyFunc<void(nx_http::StatusCode::Value, QUrl)> Handler;

    /**
     * Helper class to be used if CloudModuleEndPointFetcher user can die before
     * CloudModuleEndPointFetcher instance.
     */
    class NX_NETWORK_API ScopedOperation
    {
    public:
        ScopedOperation(CloudModuleEndPointFetcher* fetcher);
        ~ScopedOperation();

        void get(nx_http::AuthInfo auth, Handler handler);
        void get(Handler handler);

    private:
        CloudModuleEndPointFetcher* const m_fetcher;
        nx::utils::AsyncOperationGuard m_guard;
    };

    CloudModuleEndPointFetcher(
        const QString& moduleName,
        std::unique_ptr<AbstractEndpointSelector> endpointSelector);
    ~CloudModuleEndPointFetcher();

    virtual void stopWhileInAioThread() override;

    /**
     * Default value taken from application setup.
     */
    void setModulesXmlUrl(QUrl url);

    //!Specify endpoint explicitely
    void setUrl(QUrl url);
    //!Retrieves endpoint if unknown. If endpoint is known, then calls \a handler directly from this method
    void get(nx_http::AuthInfo auth, Handler handler);
    void get(Handler handler);

private:
    mutable QnMutex m_mutex;
    boost::optional<QUrl> m_endpoint;
    nx_http::AsyncHttpClientPtr m_httpClient;
    const CloudInstanceSelectionAttributeNameset m_nameset;
    const int m_moduleAttrName;
    std::vector<Handler> m_resolveHandlers;
    std::unique_ptr<AbstractEndpointSelector> m_endpointSelector;
    bool m_requestIsRunning;
    QUrl m_modulesXmlUrl;
    std::map<QString, QString> m_moduleToDefaultUrlScheme;

    void onHttpClientDone(nx_http::AsyncHttpClientPtr client);
    bool findModuleEndpoint(
        const stree::AbstractNode& treeRoot,
        const int moduleAttrName,
        QUrl* const moduleEndpoint);
    void signalWaitingHandlers(
        QnMutexLockerBase* const lk,
        nx_http::StatusCode::Value statusCode,
        const QUrl& endpoint);
    void endpointSelected(
        QnMutexLockerBase* const lk,
        nx_http::StatusCode::Value result,
        QUrl selectedEndpoint);
    QUrl buildUrl(const QString& str);
};

} // namespace cloud
} // namespace network
} // namespace nx
