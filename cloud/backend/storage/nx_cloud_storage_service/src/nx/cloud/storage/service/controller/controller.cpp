#include "controller.h"

#include "../settings.h"
#include "storage_manager.h"

namespace nx::cloud::storage::service {

Controller::Controller(const Settings& settings):
    /*m_settings(settings),*/
    m_storageManager(StorageManagerFactory::instance().create(settings))
{
}

Controller::~Controller() = default;

AbstractStorageManager* Controller::storageManager()
{
    return m_storageManager.get();
}

} // namespace nx::cloud::storage::service