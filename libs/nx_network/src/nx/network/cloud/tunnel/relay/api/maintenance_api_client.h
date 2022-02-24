// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/generic_api_client.h>

#include "relay_api_data_types.h"

namespace nx::cloud::relay::api {

/**
 * A client class to relay maintenance API.
 */
class NX_NETWORK_API MaintenanceClient:
    public nx::network::http::GenericApiClient<ResultDescriptor>
{
    using base_type = nx::network::http::GenericApiClient<ResultDescriptor>;

public:
    MaintenanceClient(const nx::utils::Url& baseUrl);

    void getRelays(nx::utils::MoveOnlyFunc<void(api::Result, api::Relays)> handler);
    std::tuple<api::Result, api::Relays> getRelays();

    void deleteRelay(
        const std::string& name,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler);

    api::Result deleteRelay(const std::string& name);
};

} // namespace nx::cloud::relay::api
