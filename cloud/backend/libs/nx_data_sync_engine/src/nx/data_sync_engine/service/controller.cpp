#include "controller.h"

#include "model.h"
#include "settings.h"

namespace nx::data_sync_engine {

Controller::Controller(
    const std::string& applicationId,
    const Settings& settings,
    Model* model)
    :
    m_syncronizationEngine(
        applicationId,
        QnUuid::createUuid(), //< moduleId. TODO.
        settings.synchronization(),
        ProtocolVersionRange::any,
        &model->queryExecutor())
{
}

SyncronizationEngine& Controller::syncronizationEngine()
{
    return m_syncronizationEngine;
}

} // namespace nx::data_sync_engine
