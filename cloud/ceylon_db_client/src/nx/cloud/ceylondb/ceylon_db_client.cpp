// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ceylon_db_client.h"

#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/socket_factory.h>

#include "api/request_paths.h"

namespace nx::cloud::ceylondb::client {

static constexpr auto kDefaultRequestTimeout = std::chrono::seconds(10);

CeylonDBClient::CeylonDBClient(const nx::Url& url, int idleConnectionsLimit):
    base_type(url, nx::network::ssl::kAcceptAnyCertificate, idleConnectionsLimit)
{
    setRequestTimeout(kDefaultRequestTimeout);
}

void CeylonDBClient::systemDeleted(
    const std::string& systemId,
    nx::MoveOnlyFunc<void(db::api::ResultCode)> handler)
{
    base_type::template makeAsyncCall<void>(
        nx::network::http::Method::delete_,
        nx::network::http::rest::substituteParameters(api::kSystemDeletedPath, {systemId}),
        {}, // query
        std::move(handler));
}

nx::network::http::ClientOptions& CeylonDBClient::httpClientOptions()
{
    return base_type::httpClientOptions();
}

CeylonDBClientFactory::CeylonDBClientFactory():
    base_type(
        [](const nx::Url& url, int idleConnectionsLimit)
        {
            return std::make_unique<CeylonDBClient>(url, idleConnectionsLimit);
        })
{
}

CeylonDBClientFactory& CeylonDBClientFactory::instance()
{
    static CeylonDBClientFactory staticInstance;
    return staticInstance;
}

} // namespace nx::cloud::ceylondb::client
