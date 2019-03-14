#pragma once

#include <memory>

#include <nx/cloud/relaying/listening_peer_manager.h>

#include "connect_session_manager.h"
#include "traffic_relay.h"

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; }
class Model;
namespace controller { class AbstractStatisticsProvider; }

class Controller
{
public:
    Controller(
        const conf::Settings& settings,
        Model* model);
    virtual ~Controller() = default;

    controller::AbstractTrafficRelay& trafficRelay();
    controller::AbstractConnectSessionManager& connectSessionManager();
    relaying::AbstractListeningPeerManager& listeningPeerManager();

    std::optional<network::HostAddress> discoverPublicAddress();

private:
    std::unique_ptr<controller::AbstractTrafficRelay> m_trafficRelay;
    std::unique_ptr<controller::AbstractConnectSessionManager> m_connectSessionManager;
    std::unique_ptr<relaying::AbstractListeningPeerManager> m_listeningPeerManager;
    Model* m_model;
    const conf::Settings& m_settings;
};

} // namespace relay
} // namespace cloud
} // namespace nx
