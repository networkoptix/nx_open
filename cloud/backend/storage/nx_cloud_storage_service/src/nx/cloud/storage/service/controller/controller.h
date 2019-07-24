#pragma once

#include <memory>

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
    ~Controller();

    BucketManager* bucketManager();
    StorageManager* storageManager();

private:
    std::unique_ptr<BucketManager> m_bucketManager;
    std::unique_ptr<StorageManager> m_storageManager;
};

} // namespace controller
} // namespace nx::cloud::storage::service
