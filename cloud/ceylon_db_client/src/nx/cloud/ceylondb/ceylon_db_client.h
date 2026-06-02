// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/result_code.h>
#include <nx/cloud/db/client/async_http_requests_executor.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>

namespace nx::cloud::ceylondb::client {

/**
 * Client for Ceylon DB Service API.
 * For details see Ceylon DB service documentation
 */
class NX_CEYLONDB_CLIENT_API AbstractCeylonDBClient
{
public:
    virtual ~AbstractCeylonDBClient() = default;

    virtual void systemDeleted(
        const std::string& systemId,
        nx::MoveOnlyFunc<void(db::api::ResultCode)> handler) = 0;
};

class NX_CEYLONDB_CLIENT_API CeylonDBClient:
    public AbstractCeylonDBClient,
    public nx::cloud::db::client::ApiRequestsExecutor
{
    using base_type = nx::cloud::db::client::ApiRequestsExecutor;

public:
    CeylonDBClient(const nx::Url& url, int idleConnectionsLimit = 0);

    void systemDeleted(
        const std::string& systemId,
        nx::MoveOnlyFunc<void(db::api::ResultCode)> handler) override;

    nx::network::http::ClientOptions& httpClientOptions();
};

using CeylonDBClientFactoryFunc = std::unique_ptr<AbstractCeylonDBClient>(const nx::Url&, int);

class NX_CEYLONDB_CLIENT_API CeylonDBClientFactory:
    public nx::utils::BasicFactory<CeylonDBClientFactoryFunc>
{
    using base_type = nx::utils::BasicFactory<CeylonDBClientFactoryFunc>;

public:
    CeylonDBClientFactory();

    static CeylonDBClientFactory& instance();
};

} // namespace nx::cloud::ceylondb::client
