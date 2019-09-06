#pragma once

#include <string>

#include <nx/network/http/http_types.h>

#include <nx/vms/api/data/peer_data.h>

namespace nx::clusterdb::engine::transport {

class ConnectionRequestAttributes
{
public:
    std::string connectionId;
    vms::api::PeerData remotePeer;
    std::string contentEncoding;
    int remotePeerProtocolVersion = 0;
    /** Value of User-Agent or Server HTTP header. */
    std::string nodeName;

    void write(network::http::HttpHeaders* headers);

    /**
     * @return false if required data was not found.
     */
    bool read(const network::http::HttpHeaders& headers);
};

bool fetchDataFromConnectRequest(
    const nx::network::http::Request& request,
    ConnectionRequestAttributes* connectionRequestAttributes);

} // namespace nx::clusterdb::engine::transport
