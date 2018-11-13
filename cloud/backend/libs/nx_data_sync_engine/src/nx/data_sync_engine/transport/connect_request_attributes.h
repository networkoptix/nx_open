#pragma once

#include <string>

#include <nx/network/http/http_types.h>

#include <nx/vms/api/data/peer_data.h>

namespace nx::data_sync_engine::transport {

struct ConnectionRequestAttributes
{
    std::string connectionId;
    vms::api::PeerData remotePeer;
    std::string contentEncoding;
    int remotePeerProtocolVersion = 0;
};

bool fetchDataFromConnectRequest(
    const nx::network::http::Request& request,
    ConnectionRequestAttributes* connectionRequestAttributes);

} // namespace nx::data_sync_engine::transport
