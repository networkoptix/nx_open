#pragma once

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

private:
    controller::TrafficRelay m_trafficRelay;
    controller::ConnectSessionManager m_connectSessionManager;
};

} // namespace relay
} // namespace cloud
} // namespace nx
