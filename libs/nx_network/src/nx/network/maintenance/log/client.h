#pragma once

#include <nx/network/http/generic_api_client.h>

#include "logger.h"

namespace nx::network::maintenance::log {

class NX_NETWORK_API Client:
    public nx::network::http::GenericApiClient<Client>
{
    using base_type = nx::network::http::GenericApiClient<Client>;

public:
    using ResultCode = nx::network::http::StatusCode::Value;

    Client(const nx::utils::Url& baseApiUrl);
    ~Client();

    void getConfiguration(
        nx::utils::MoveOnlyFunc<void(ResultCode, Loggers)> completionHandler);

    std::tuple<ResultCode, Loggers> getConfiguration();
};

} // namespace nx::network::maintenance::log
