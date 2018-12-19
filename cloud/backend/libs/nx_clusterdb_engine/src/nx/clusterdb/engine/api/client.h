#pragma once

#include <string>
#include <tuple>

#include <nx/network/http/generic_api_client.h>

#include "peer_info.h"

namespace nx::clusterdb::engine::api {

class NX_DATA_SYNC_ENGINE_API Client:
    public nx::network::http::GenericApiClient<Client>
{
    using base_type = nx::network::http::GenericApiClient<Client>;

public:
    using ResultCode = nx::network::http::StatusCode::Value;

    Client(
        const nx::utils::Url& baseApiUrl,
        const std::string& systemId);
    ~Client();

    void getInfo(
        nx::utils::MoveOnlyFunc<void(ResultCode, PeerInfo)> completionHandler);

    std::tuple<ResultCode, PeerInfo> getInfo();

private:
    const std::string m_systemId;
};

} // namespace nx::clusterdb::engine::api
