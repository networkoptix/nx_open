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
        settings,
        &model->clientSessionPool(),
        &model->listeningPeerPool())
{
}

} // namespace relay
} // namespace cloud
} // namespace nx
