#include "controller.h"

#include "nx/cloud/storage/service/model/model.h"
#include "nx/cloud/storage/service/settings.h"
#include "bucket_manager.h"
#include "storage_manager.h"

namespace nx::cloud::storage::service::controller {

Controller::Controller(const conf::Settings& settings, model::Model* /*model*/):
    m_bucketManager(std::make_unique<BucketManager>(settings.aws())),
    m_storageManager(std::make_unique<StorageManager>(settings.aws()))
{
}

Controller::~Controller() = default;

BucketManager* Controller::bucketManager()
{
    return m_bucketManager.get();
}

StorageManager* Controller::storageManager()
{
    return m_storageManager.get();
}

} // namespace nx::cloud::storage::service::controller
