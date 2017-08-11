#include "controller.h"
#include "../model/model.h"

namespace nx {
namespace cloud {
namespace relay {

Controller::Controller(
    const conf::Settings& settings,
    Model* model)
    :
    m_connectSessionManager(
        controller::ConnectSessionManagerFactory::create(
            settings,
            &model->clientSessionPool(),
            &model->listeningPeerPool(),
            &m_trafficRelay))
{
}

controller::AbstractConnectSessionManager& Controller::connectSessionManager()
{
    return *m_connectSessionManager;
}

} // namespace relay
} // namespace cloud
} // namespace nx
