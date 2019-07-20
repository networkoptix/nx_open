#include "storage_manager.h"

#include "../settings.h"

namespace nx::cloud::storage::service {

void StorageManager::addStorage(
    const api::AddStorageRequest& request,
    nx::utils::MoveOnlyFunc<void(api::Result, api::AddStorageResponse)> handler)
{
    api::AddStorageResponse response;
    response.totalSpace = request.size;
    response.freeSpace = request.size;
    handler(api::Result{api::ResultCode::ok, "addOk"}, std::move(response));
}

void StorageManager::readStorage(
    const std::string& storageId,
    nx::utils::MoveOnlyFunc<void(api::Result, api::ReadStorageResponse)> handler)
{
    api::ReadStorageResponse response;
    response.id = storageId;
    handler(api::Result{api::ResultCode::ok, "readOk"}, std::move(response));
}

void StorageManager::removeStorage(
    const std::string& /*storageId*/,
    nx::utils::MoveOnlyFunc<void(api::Result)> handler)
{
    handler(api::Result{api::ResultCode::ok, "removeOk"});
}

StorageManagerFactory::StorageManagerFactory():
    base_type(
        std::bind(&StorageManagerFactory::defaultFactoryFunction, this, std::placeholders::_1))
{
}

StorageManagerFactory& StorageManagerFactory::instance()
{
    static StorageManagerFactory factory;
    return factory;
}

std::unique_ptr<AbstractStorageManager> StorageManagerFactory::defaultFactoryFunction(
    const Settings& /*settings*/)
{
    return std::make_unique<StorageManager>();
}

} // namespace nx::cloud::storage::service