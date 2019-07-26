#include "controller.h"

#include "nx/cloud/storage/service/model/model.h"
#include "nx/cloud/storage/service/settings.h"

namespace nx::cloud::storage::service::controller {

Controller::Controller(const conf::Settings& settings, model::Model* model):
    m_bucketManager(settings, model),
    m_storageManager(settings, model, &m_bucketManager)
{
}

BucketManager* Controller::bucketManager()
{
    return &m_bucketManager;
}

StorageManager* Controller::storageManager()
{
    return &m_storageManager;
}

} // namespace nx::cloud::storage::service::controller
