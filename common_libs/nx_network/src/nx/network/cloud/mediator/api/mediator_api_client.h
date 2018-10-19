#pragma once

#include <memory>
#include <vector>

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

    using ListeningPeersHandler =
        nx::utils::MoveOnlyFunc<void(ResultCode, ListeningPeers)>;

    Client(const nx::utils::Url& baseMediatorApiUrl);
    ~Client();

    void getListeningPeers(ListeningPeersHandler completionHandler);

    std::tuple<ResultCode, ListeningPeers> getListeningPeers();
};

} // namespace api
} // namespace hpm
} // namespace nx
