#pragma once

#include <memory>
#include <vector>

#include <nx/network/cloud/data/connect_data.h>
#include <nx/network/http/generic_api_client.h>

#include "listening_peer.h"

namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API Client:
    public nx::network::http::GenericApiClient<Client>
{
    using base_type = nx::network::http::GenericApiClient<Client>;

public:
    using ResultCode = nx::network::http::StatusCode::Value;

    Client(const nx::utils::Url& baseMediatorApiUrl);
    ~Client();

    void getListeningPeers(
        nx::utils::MoveOnlyFunc<void(ResultCode, ListeningPeers)> completionHandler);

    std::tuple<ResultCode, ListeningPeers> getListeningPeers();

    void initiateConnection(
        const ConnectRequest& request,
        nx::utils::MoveOnlyFunc<void(ResultCode, ConnectResponse)> completionHandler);
};

} // namespace api
} // namespace hpm
} // namespace nx
