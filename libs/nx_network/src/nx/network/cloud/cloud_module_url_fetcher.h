// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <map>
#include <list>
#include <optional>
#include <string>

#include <nx/utils/async_operation_guard.h>
#include <nx/utils/move_only_func.h>

#include "basic_cloud_module_url_fetcher.h"
#include "endpoint_selector.h"

namespace nx::network::cloud {

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

        void get(
            nx::network::http::AuthInfo auth,
            nx::network::ssl::AdapterFunc proxyAdapterFunc,
            Handler handler);

        void get(Handler handler);

    private:
        CloudModuleUrlFetcher* const m_fetcher;
        nx::utils::AsyncOperationGuard m_guard;
    };

    CloudModuleUrlFetcher(std::string moduleName);

    /**
     * Retrieves endpoint if unknown.
     * If endpoint is known, then calls handler directly from this method.
     */
    void get(
        nx::network::http::AuthInfo auth,
        nx::network::ssl::AdapterFunc proxyAdapterFunc,
        Handler handler);

    void get(Handler handler);

    /**
     * Specify url explicitly.
     */
    void setUrl(nx::utils::Url endpoint);

    /**
     * Reset cached url value.
     */
    void resetUrl();

protected:
    virtual bool analyzeXmlSearchResult(
        const nx::utils::stree::AttributeDictionary& searchResult) override;

    virtual void invokeHandler(
        const Handler& handler,
        nx::network::http::StatusCode::Value statusCode) override;

private:
    mutable nx::Mutex m_mutex;
    const std::string m_moduleAttrName;
    std::optional<nx::utils::Url> m_url;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API CloudDbUrlFetcher:
    public CloudModuleUrlFetcher
{
public:
    CloudDbUrlFetcher();
};

} // namespace nx::network::cloud
