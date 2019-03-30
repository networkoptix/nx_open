#include "controller.h"

#include "model.h"
#include "settings.h"

namespace nx::clusterdb::engine {

Controller::Controller(
    const std::string& applicationId,
    const Settings& settings,
    const ProtocolVersionRange& protocolVersionRange,
    Model* model)
    :
    m_synchronizationEngine(
        applicationId,
        settings.synchronization(),
        protocolVersionRange,
        &model->queryExecutor())
{
}

SynchronizationEngine& Controller::synchronizationEngine()
{
    return m_synchronizationEngine;
}

} // namespace nx::clusterdb::engine
