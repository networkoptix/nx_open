// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "edge_db_connection.h"

#include <nx/cloud/db/client/async_http_requests_executor.h>
#include <nx/cloud/db/client/cdb_connection.h>
#include <nx/network/url/url_builder.h>
#include <nx/vms/client/core/application_context.h>

#include "cloud_status_watcher.h"

namespace nx::vms::client::core {

using ContextPtr = nx::network::http::ClientPool::ContextPtr;

struct EdgeDbConnection::Private
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

    ContextPtr prepareContext(nx::network::http::ClientPool::Request&& request,
        std::function<void(ContextPtr context)>&& callback) const
    {
        auto result = httpClientPool->createContext(
            certificateFuncId, nx::network::ssl::kDefaultCertificateCheck);
        result->request = std::move(request);
        result->completionFunc = std::move(callback);

        return result;
    }
};

EdgeDbConnection::EdgeDbConnection():
    d(new Private{
        .httpClientPool = std::make_unique<nx::network::http::ClientPool>(),
        .cloudStatusWatcher = appContext()->cloudStatusWatcher(),
    })
{
}

EdgeDbConnection::~EdgeDbConnection()
{
}

rest::Handle EdgeDbConnection::sendGet(
    const QString& path, const nx::network::rest::Params& params, RawCallback callback)
{
    if (d->cloudStatusWatcher->status() != CloudStatusWatcher::Status::Online)
    {
        NX_DEBUG(this, "Cloud is not online, request %1 is not sent", path);
        return {};
    }

    auto completion = [callback = std::move(callback)](ContextPtr context)
    {
        std::optional<nx::network::http::Response> response;
        if (context->hasResponse())
            response = std::move(context->response);

        callback(context->handle, context->systemError, std::move(response));
    };

    return d->httpClientPool->sendRequest(d->prepareContext(
        d->prepareRequest(nx::network::http::Method::get, d->prepareUrl(path, params)),
        std::move(completion)));
}

nx::network::rest::Result EdgeDbConnection::parseError(const nx::network::http::Response& response)
{
    using nx::network::rest::Result;

    const auto reasonPhrase = QString::fromStdString(response.statusLine.reasonPhrase);
    auto [result, deserialized] = nx::reflect::json::deserialize<Result>(response.messageBody);

    // A body without the edgeDb error, e.g. of a proxy, leaves the error of the HTTP status.
    if (!deserialized || result.errorId == nx::network::rest::ErrorId::ok)
        return Result(Result::errorFromHttpStatus(response.statusLine.statusCode), reasonPhrase);

    if (result.errorString.isEmpty())
        result.errorString = reasonPhrase;

    return result;
}

void EdgeDbConnection::cancel(rest::Handle handle)
{
    d->httpClientPool->terminate(handle);
}

} // namespace nx::vms::client::core
