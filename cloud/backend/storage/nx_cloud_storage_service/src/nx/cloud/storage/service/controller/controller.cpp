#include "controller.h"

#include "nx/cloud/storage/service/model/model.h"
#include "nx/cloud/storage/service/settings.h"

namespace nx::cloud::storage::service::controller {

Controller::Controller(const conf::Settings& settings, model::Model* model):
    m_bucketManager(settings, model),
    m_storageManager(settings, model, &m_bucketManager)
{
}

void Controller::stop()
{
    NX_INFO(this, "Stopping Controller...");
    m_storageManager.stop();
    m_bucketManager.stop();
    NX_INFO(this, "Controller stropped");
}

BucketManager& Controller::bucketManager()
{
    return m_bucketManager;
}

StorageManager& Controller::storageManager()
{
    return m_storageManager;
}

} // namespace nx::cloud::storage::service::controller
