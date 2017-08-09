#pragma once

#include <string>
#include "connect_session_manager.h"
#include "traffic_relay.h"

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; }
class Model;

class Controller
{
public:
    Controller(
        const conf::Settings& settings,
        Model* model);

    controller::AbstractConnectSessionManager& connectSessionManager();

private:
    controller::TrafficRelay m_trafficRelay;
    std::unique_ptr<controller::AbstractConnectSessionManager> m_connectSessionManager;

    std::string discoverPublicIp(const conf::Settings& settings);
};

} // namespace relay
} // namespace cloud
} // namespace nx
