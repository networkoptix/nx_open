#pragma once

#include <nx/utils/move_only_func.h>

#include <nx/network/cloud/storage/service/api/add_storage.h>
#include <nx/network/cloud/storage/service/api/read_storage.h>
#include <nx/network/cloud/storage/service/api/result.h>
#include <nx/utils/basic_factory.h>

namespace nx::cloud::storage::service {

class Settings;

class AbstractStorageManager
{
public:
    virtual ~AbstractStorageManager() = default;

    virtual void addStorage(
        const api::AddStorageRequest& request,
        nx::utils::MoveOnlyFunc<void(api::Result, api::AddStorageResponse)> handler) = 0;

    virtual void readStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result, api::ReadStorageResponse)> handler) = 0;

    virtual void removeStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler) = 0;
};

//-------------------------------------------------------------------------------------------------
// StorageManager

class StorageManager:
    public AbstractStorageManager
{
public:
    virtual void addStorage(
        const api::AddStorageRequest& request,
        nx::utils::MoveOnlyFunc<void(api::Result, api::AddStorageResponse)> handler) override;

    virtual void readStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result, api::ReadStorageResponse) > handler) override;

    virtual void removeStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler) override;
};

//-------------------------------------------------------------------------------------------------
// StorageManagerFactory

using StorageManagerFactoryFunc = std::unique_ptr<AbstractStorageManager>(const Settings&);

class StorageManagerFactory:
    public nx::utils::BasicFactory<StorageManagerFactoryFunc>
{
    using base_type = nx::utils::BasicFactory<StorageManagerFactoryFunc>;
public:
    StorageManagerFactory();

    static StorageManagerFactory& instance();

private:
    std::unique_ptr<AbstractStorageManager> defaultFactoryFunction(const Settings& settings);
};

}