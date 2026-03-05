// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vmsdb_connection.h"

#include <nx/cloud/db/client/async_http_requests_executor.h>
#include <nx/cloud/db/client/cdb_connection.h>
#include <nx/network/url/url_builder.h>
#include <nx/vms/client/core/application_context.h>

#include "cloud_status_watcher.h"

namespace nx::vms::client::core {

struct VmsDbConnection::Private
{
    std::unique_ptr<nx::network::http::ClientPool> httpClientPool;
    CloudStatusWatcher* cloudStatusWatcher = nullptr;

    /** Unique certificate func id to reuse connections. */
    const nx::Uuid certificateFuncId = nx::Uuid::createUuid();


    nx::Url prepareUrl(const QString& path, const nx::network::rest::Params& params) const
    {
        return nx::network::url::Builder(
            cloudStatusWatcher->cloudConnection()->requestsExecutor()->baseApiUrl())
            .setPath(path)
            .setQuery(params.toUrlQuery())
            .toUrl();
    }

    nx::network::http::ClientPool::Request prepareRequest(
        nx::network::http::Method&& method, nx::Url&& url) const
    {
        return nx::network::http::ClientPool::Request{
            .method = std::move(method),
            .url = std::move(url),
            .authType = nx::network::http::AuthType::authBearer,
            .credentials = cloudStatusWatcher->credentials(),
        };
    }

    ContextPtr prepareContext(
        nx::network::http::ClientPool::Request&& request,
        std::function<void(ContextPtr context)>&& callback) const
    {
        auto result = httpClientPool->createContext(
            certificateFuncId, nx::network::ssl::kDefaultCertificateCheck);
        result->request = std::move(request);
        result->completionFunc = std::move(callback);

        return result;
    }
};

VmsDbConnection::VmsDbConnection()
    : d(new Private{
        .httpClientPool = std::make_unique<nx::network::http::ClientPool>(),
        .cloudStatusWatcher = appContext()->cloudStatusWatcher(),
        })
{
}

VmsDbConnection::~VmsDbConnection()
{
}

rest::Handle VmsDbConnection::request(
    nx::network::http::Method method,
    const QString& path,
    const nx::network::rest::Params& params,
    std::function<void(VmsDbConnection::ContextPtr context)>&& callback)
{
    if (d->cloudStatusWatcher->status() != CloudStatusWatcher::Status::Online)
        return {};

    return d->httpClientPool->sendRequest(d->prepareContext(
        d->prepareRequest(std::move(method), d->prepareUrl(path, params)),
        std::move(callback)
    ));
}

} // namespace nx::vms::client::core
