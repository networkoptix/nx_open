// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "maintenance_api_client.h"

#include <nx/network/http/rest/http_rest_client.h>

#include "relay_api_http_paths.h"

namespace nx::cloud::relay::api {

MaintenanceClient::MaintenanceClient(const nx::utils::Url& baseUrl):
    base_type(baseUrl, nx::network::ssl::kDefaultCertificateCheck)
{
}

void MaintenanceClient::getRelays(
    nx::utils::MoveOnlyFunc<void(api::Result, api::Relays)> handler)
{
    base_type::template makeAsyncCall<api::Relays>(
        nx::network::http::Method::get,
        kDbRelaysPath,
        {}, // query
        std::move(handler));
}

std::tuple<api::Result, api::Relays> MaintenanceClient::getRelays()
{
    return base_type::template makeSyncCall<api::Relays>(
        nx::network::http::Method::get,
        kDbRelaysPath,
        nx::utils::UrlQuery()); // query
}

void MaintenanceClient::deleteRelay(
    const std::string& name,
    nx::utils::MoveOnlyFunc<void(api::Result)> handler)
{
    base_type::template makeAsyncCall<void>(
        nx::network::http::Method::delete_,
        nx::network::http::rest::substituteParameters(kDbRelayPath, {name}),
        {}, // query
        std::move(handler));
}

api::Result MaintenanceClient::deleteRelay(const std::string& name)
{
    return std::get<0>(base_type::template makeSyncCall<void>(
        nx::network::http::Method::delete_,
        nx::network::http::rest::substituteParameters(kDbRelayPath, {name}),
        nx::utils::UrlQuery()));
}

} // namespace nx::cloud::relay::api
