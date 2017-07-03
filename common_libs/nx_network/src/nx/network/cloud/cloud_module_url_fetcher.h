#pragma once

#include <functional>
#include <map>
#include <list>

#include <boost/optional.hpp>

#include <QtCore/QString>

#include <nx/utils/async_operation_guard.h>
#include <nx/utils/move_only_func.h>

#include "basic_cloud_module_url_fetcher.h"
#include "endpoint_selector.h"

namespace nx {
namespace network {
namespace cloud {

/**
 * Fetches single url of a specified name.
 */
class NX_NETWORK_API CloudModuleUrlFetcher:
    public BasicCloudModuleUrlFetcher<
        nx::utils::MoveOnlyFunc<void(nx_http::StatusCode::Value, QUrl)>>
{
    using base_type = BasicCloudModuleUrlFetcher<
        nx::utils::MoveOnlyFunc<void(nx_http::StatusCode::Value, QUrl)>>;

public:
    using Handler = nx::utils::MoveOnlyFunc<void(nx_http::StatusCode::Value, QUrl)>;

    /**
     * Helper class to be used if BasicCloudModuleUrlFetcher user can die before
     *     BasicCloudModuleUrlFetcher instance.
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

    /**
     * Retrieves endpoint if unknown. 
     * If endpoint is known, then calls handler directly from this method.
     */
    void get(nx_http::AuthInfo auth, Handler handler);
    void get(Handler handler);
    /**
     * Specify url explicitly.
     */
    void setUrl(QUrl endpoint);

protected:
    virtual void stopWhileInAioThread() override;
    virtual bool analyzeXmlSearchResult(
        const nx::utils::stree::ResourceContainer& searchResult) override;
    virtual void invokeHandler(
        const Handler& handler,
        nx_http::StatusCode::Value statusCode) override;

private:
    std::unique_ptr<AbstractEndpointSelector> m_endpointSelector;
    const int m_moduleAttrName;
    boost::optional<QUrl> m_url;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API CloudDbUrlFetcher:
    public CloudModuleUrlFetcher
{
public:
    CloudDbUrlFetcher(
        std::unique_ptr<AbstractEndpointSelector> endpointSelector);
};

} // namespace cloud
} // namespace network
} // namespace nx
