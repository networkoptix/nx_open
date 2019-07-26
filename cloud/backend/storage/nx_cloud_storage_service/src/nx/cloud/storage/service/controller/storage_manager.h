#pragma once

#include <nx/utils/move_only_func.h>

#include <nx/network/cloud/storage/service/api/add_storage.h>
#include <nx/network/cloud/storage/service/api/storage.h>
#include <nx/network/cloud/storage/service/api/result.h>
#include <nx/utils/basic_factory.h>

namespace nx::cloud::storage::service {

namespace conf { class Settings; } // namespace conf

namespace model {

namespace dao { class AbstractStorageDao; }
class Database;
class Model;

} // namespace model

namespace controller {

class BucketManager;

class StorageManager
{
public:
    StorageManager(
        const conf::Settings& settings,
        model::Model* model,
        BucketManager* bucketManager);

    void addStorage(
        const api::AddStorageRequest& request,
        nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)> handler);

    void readStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)> handler);

    void removeStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler);

    void listCameras(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result, std::vector<std::string>)> handler);
private:
    /*const conf::Settings& m_settings;
    model::Database* m_database = nullptr;
    model::dao::AbstractStorageDao* m_storageDao = nullptr;
    BucketManager* m_bucketManager = nullptr;*/
};

} // namespace controller
} // namespace nx::cloud::storage::service
