#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <nx/network/websocket/websocket.h>

#include "discovery_client.h"
#include "discovery_settings.h"

namespace nx {
namespace cloud {
namespace discovery {

class RegisteredPeerPool
{
public:
    RegisteredPeerPool(const conf::Discovery& configuration);

    /**
     * If connection from peer with id peerId is already present, it is replaced.
     * Each message inside websocket is treated as JSON representation of 
     * BasicInstanceInformation descendant.
     * When connection breaks peer is still considered online for Discovery::keepAliveTimeout.
     */
    void addPeerConnection(
        const std::string& peerId,
        std::unique_ptr<nx::network::WebSocket> connection);
    /**
     * @return List of module information in JSON representation.
     */
    std::vector<std::string> getPeerInfoList(const std::string& peerTypeName) const;
    /**
     * @return JSON representation of module information.
     */
    std::string getPeerInfo(const std::string& peerId) const;

private:
    const conf::Discovery m_configuration;
};

} // namespace nx
} // namespace cloud
} // namespace discovery
