#pragma once

#include <memory>

namespace nx::cloud::storage::service {

namespace conf { class Settings; }
namespace bucket { class AbstractManager; }
namespace storage { class AbstractManager; }
class Model;

class Controller
{
public:
    Controller(const conf::Settings& settings, Model* model);
    ~Controller();

    bucket::AbstractManager* bucketManager();
    storage::AbstractManager* storageManager();

private:
    std::unique_ptr<bucket::AbstractManager> m_bucketManager;
    std::unique_ptr<storage::AbstractManager> m_storageManager;
};

} // namespace nx::cloud::storage::service