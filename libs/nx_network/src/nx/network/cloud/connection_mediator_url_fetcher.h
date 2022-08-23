// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include "basic_cloud_module_url_fetcher.h"

namespace nx::network::cloud {

using MediatorUrlHandler = nx::utils::MoveOnlyFunc<void(
    nx::network::http::StatusCode::Value /*result code*/,
    nx::utils::Url /*tcp url*/,
    nx::utils::Url /*udp url*/)>;

class NX_NETWORK_API ConnectionMediatorUrlFetcher:
    public BasicCloudModuleUrlFetcher<MediatorUrlHandler>
{
    using base_type = BasicCloudModuleUrlFetcher<MediatorUrlHandler>;

public:
    using Handler = MediatorUrlHandler;

    /**
     * Retrieves endpoint if unknown.
     * If endpoint is known, then calls handler directly from this method.
     */
    void get(
        nx::network::http::AuthInfo auth,
        nx::network::ssl::AdapterFunc adapterFunc,
        Handler handler);

    void get(Handler handler);

protected:
    virtual bool analyzeXmlSearchResult(
        const nx::utils::stree::AttributeDictionary& searchResult) override;

    virtual void invokeHandler(
        const Handler& handler,
        nx::network::http::StatusCode::Value statusCode) override;

private:
    struct MediatorHostDescriptor
    {
        nx::utils::Url tcpUrl;
        nx::utils::Url udpUrl;
    };

    std::optional<MediatorHostDescriptor> m_mediatorHostDescriptor;
    mutable nx::Mutex m_mutex;
};

} // namespace nx::network::cloud
