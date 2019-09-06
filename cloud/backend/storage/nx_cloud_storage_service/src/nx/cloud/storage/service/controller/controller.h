#pragma once

#include "bucket_manager.h"
#include "storage_manager.h"

namespace nx::cloud::storage::service {

namespace conf { class Settings; }
namespace model { class Model; }

namespace controller {

class BucketManager;
class StorageManager;

class Controller
{
public:
    Controller(const conf::Settings& settings, model::Model* model);

    void stop();

    BucketManager& bucketManager();
    StorageManager& storageManager();

private:
    BucketManager m_bucketManager;
    StorageManager m_storageManager;
};

} // namespace controller
} // namespace nx::cloud::storage::service
