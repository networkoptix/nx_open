// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/generic_api_client.h>

#include "logger.h"

namespace nx::network::maintenance::log {

class NX_NETWORK_API Client:
    public nx::network::http::DefaultGenericApiClient
{
    using base_type = nx::network::http::DefaultGenericApiClient;

public:
    using ResultCode = nx::network::http::StatusCode::Value;

    Client(const nx::utils::Url& baseApiUrl);
    ~Client();

    void getConfiguration(
        nx::utils::MoveOnlyFunc<void(ResultCode, LoggerList)> completionHandler);

    std::tuple<ResultCode, LoggerList> getConfiguration();
};

} // namespace nx::network::maintenance::log
