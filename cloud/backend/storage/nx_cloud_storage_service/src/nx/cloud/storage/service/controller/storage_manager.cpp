#include "storage_manager.h"

#include "nx/cloud/storage/service/settings.h"
#include "nx/cloud/storage/service/model/model.h"
#include "nx/cloud/storage/service/model/database.h"
#include "nx/cloud/storage/service/model/dao/storage_dao.h"

namespace nx::cloud::storage::service::controller {

StorageManager::StorageManager(
    const conf::Settings& /*settings*/,
    model::Model* /*model*/,
    BucketManager* /*bucketManager*/)
    /*:
    m_settings(settings),
    m_database(model->database()),
    m_storageDao(model->storageDao()),
    m_bucketManager(bucketManager)*/
{
}

void StorageManager::addStorage(
    const api::AddStorageRequest& request,
    nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)> handler)
{
    api::Storage response;
    response.totalSpace = request.totalSpace;
    response.freeSpace = request.totalSpace;
    handler(api::Result{api::ResultCode::ok, "addOk"}, std::move(response));
}

void StorageManager::readStorage(
    const std::string& storageId,
    nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)> handler)
{
    api::Storage response;
    response.id = storageId;
    handler(api::Result{api::ResultCode::ok, "readOk"}, std::move(response));
}

void StorageManager::removeStorage(
    const std::string& /*storageId*/,
    nx::utils::MoveOnlyFunc<void(api::Result)> handler)
{
    handler(api::Result{api::ResultCode::ok, "removeOk"});
}

void StorageManager::listCameras(
    const std::string& /*storageId*/,
    nx::utils::MoveOnlyFunc<void(api::Result, std::vector<std::string>)> handler)
{
    handler(api::Result{api::ResultCode::ok, "listCamerasOk"}, std::vector<std::string>());
}

} // namespace nx::cloud::storage::service::controller
