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
        nx::utils::MoveOnlyFunc<void(nx::network::http::StatusCode::Value, nx::utils::Url)>>
{
    using base_type = BasicCloudModuleUrlFetcher<
        nx::utils::MoveOnlyFunc<void(nx::network::http::StatusCode::Value, nx::utils::Url)>>;

public:
    using Handler = nx::utils::MoveOnlyFunc<void(nx::network::http::StatusCode::Value, nx::utils::Url)>;

    /**
     * Helper class to be used if BasicCloudModuleUrlFetcher user can die before
     *     BasicCloudModuleUrlFetcher instance.
     */
    class NX_NETWORK_API ScopedOperation
    {
    public:
        ScopedOperation(CloudModuleUrlFetcher* fetcher);
        ~ScopedOperation();

        void get(nx::network::http::AuthInfo auth, Handler handler);
        void get(Handler handler);

    private:
        CloudModuleUrlFetcher* const m_fetcher;
        nx::utils::AsyncOperationGuard m_guard;
    };

    CloudModuleUrlFetcher(const QString& moduleName);

    /**
     * Retrieves endpoint if unknown.
     * If endpoint is known, then calls handler directly from this method.
     */
    void get(nx::network::http::AuthInfo auth, Handler handler);
    void get(Handler handler);
    /**
     * Specify url explicitly.
     */
    void setUrl(nx::utils::Url endpoint);

protected:
    virtual bool analyzeXmlSearchResult(
        const nx::utils::stree::ResourceContainer& searchResult) override;
    virtual void invokeHandler(
        const Handler& handler,
        nx::network::http::StatusCode::Value statusCode) override;

private:
    const int m_moduleAttrName;
    boost::optional<nx::utils::Url> m_url;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API CloudDbUrlFetcher:
    public CloudModuleUrlFetcher
{
public:
    CloudDbUrlFetcher();
};

} // namespace cloud
} // namespace network
} // namespace nx
