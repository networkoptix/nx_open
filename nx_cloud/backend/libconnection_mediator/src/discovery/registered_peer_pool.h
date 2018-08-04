#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <boost/optional.hpp>

#include <nx/network/websocket/websocket.h>
#include <nx/utils/thread/mutex.h>

#include "discovery_client.h"
#include "discovery_settings.h"

namespace nx {
namespace cloud {
namespace discovery {

struct DummyInstanceInformation:
    public BasicInstanceInformation
{
    DummyInstanceInformation():
        BasicInstanceInformation("dummy")
    {
    }
};

class RegisteredPeerPool
{
public:
    RegisteredPeerPool(const conf::Discovery& configuration);
    ~RegisteredPeerPool();

    /**
     * If connection from peer with id peerId is already present, it is replaced.
     * Each message inside websocket is treated as JSON representation of
     * BasicInstanceInformation descendant.
     * When connection has been broken peer is still considered online for Discovery::keepAliveTimeout.
     */
    void addPeerConnection(
        const std::string& peerId,
        std::unique_ptr<nx::network::WebSocket> connection);
    /**
     * @return List of module information in JSON representation.
     */
    std::vector<std::string> getPeerInfoListByType(const std::string& peerTypeName) const;
    std::vector<std::string> getPeerInfoList() const;
    /**
     * @return JSON representation of module information.
     */
    boost::optional<std::string> getPeerInfo(const std::string& peerId) const;

private:
    struct PeerContext
    {
        std::unique_ptr<nx::network::WebSocket> connection;
        std::string moduleInformationJson;
        boost::optional<DummyInstanceInformation> info;
        nx::Buffer readBuffer;
    };

    const conf::Discovery m_configuration;
    std::map<std::string, std::unique_ptr<PeerContext>> m_peerIdToConnection;
    mutable QnMutex m_mutex;

    void onBytesRead(
        const std::string& peerId,
        SystemError::ErrorCode systemErrorCode,
        std::size_t bytesRead);
    bool isPeerInfoValid(
        const std::string& peerId,
        const DummyInstanceInformation& serializedInfo) const;
};

} // namespace discovery
} // namespace cloud
} // namespace nx
