#include "controller.h"

#include "model.h"
#include "settings.h"

namespace nx::clusterdb::engine {

Controller::Controller(
    const std::string& applicationId,
    const Settings& settings,
    Model* model)
    :
    m_syncronizationEngine(
        applicationId,
        settings.synchronization(),
        ProtocolVersionRange::any,
        &model->queryExecutor())
{
}

SyncronizationEngine& Controller::syncronizationEngine()
{
    return m_syncronizationEngine;
}

} // namespace nx::clusterdb::engine
