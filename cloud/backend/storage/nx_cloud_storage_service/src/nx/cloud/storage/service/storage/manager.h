#pragma once

#include <nx/utils/move_only_func.h>

#include <nx/network/cloud/storage/service/api/add_storage.h>
#include <nx/network/cloud/storage/service/api/read_storage.h>
#include <nx/network/cloud/storage/service/api/result.h>
#include <nx/utils/basic_factory.h>

namespace nx::cloud::storage::service {

namespace conf { class Settings; }
class Database;

namespace storage {

class AbstractManager
{
public:
    virtual ~AbstractManager() = default;

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
// Manager

class Manager:
    public AbstractManager
{
public:
    Manager(Database* database);

    virtual void addStorage(
        const api::AddStorageRequest& request,
        nx::utils::MoveOnlyFunc<void(api::Result, api::AddStorageResponse)> handler) override;

    virtual void readStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result, api::ReadStorageResponse)> handler) override;

    virtual void removeStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler) override;

private:
    Database* m_database;
};

//-------------------------------------------------------------------------------------------------
// ManagerFactory

using FactoryType = std::unique_ptr<AbstractManager>(const conf::Settings&, Database*);

class ManagerFactory:
    public nx::utils::BasicFactory<FactoryType>
{
    using base_type = nx::utils::BasicFactory<FactoryType>;

public:
    ManagerFactory();

    static ManagerFactory& instance();

private:
    std::unique_ptr<AbstractManager> defaultFactoryFunction(
        const conf::Settings& settings,
        Database* database);
};

} // namespace storage
} // namespace nx::cloud::storage::service