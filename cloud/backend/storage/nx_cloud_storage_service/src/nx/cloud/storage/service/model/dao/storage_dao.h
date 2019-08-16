#pragma once

#include <nx/utils/basic_factory.h>

namespace nx::sql { class QueryContext; }

namespace nx::cloud::storage::service {

namespace api {

struct Device;
struct Storage;
struct System;

} // namespace api

namespace model {

namespace dao {

class AbstractStorageDao
{
public:
    virtual ~AbstractStorageDao() = default;

    /**
     * NOTE: Throws exception if storage.ioDevices does not contain at least one entry
     */
    virtual void addStorage(
        nx::sql::QueryContext* queryContext,
        const api::Storage& storage) = 0;

    virtual std::optional<api::Storage> readStorage(
        nx::sql::QueryContext* queryContext,
        const std::string& storageId) = 0;

    virtual void removeStorage(
        nx::sql::QueryContext* queryContext,
        const std::string& storageId) = 0;

    virtual void addSystem(
        nx::sql::QueryContext* queryContext,
        const api::System& system) = 0;

    virtual void removeSystem(
        nx::sql::QueryContext* queryContext,
        const api::System& system) = 0;
};

//-------------------------------------------------------------------------------------------------
// StorageDao

class StorageDao:
    public AbstractStorageDao
{
public:
    virtual void addStorage(
        nx::sql::QueryContext* queryContext,
        const api::Storage& storage) override;

    virtual std::optional<api::Storage> readStorage(
        nx::sql::QueryContext* queryContext,
        const std::string& storageId) override;

    virtual void removeStorage(
        nx::sql::QueryContext* queryContext,
        const std::string& storageId) override;

    virtual void addSystem(
        nx::sql::QueryContext* queryContext,
        const api::System& system) override;

    virtual void removeSystem(
        nx::sql::QueryContext* queryContext,
        const api::System& system) override;

private:
    void addStorageBucketRelation(
        nx::sql::QueryContext* queryContext,
        const api::Storage& storage);

    std::vector<api::Device> getStorageDevices(
        nx::sql::QueryContext* queryContext,
        const std::string& storageId);

    std::vector<std::string> getSystems(
        nx::sql::QueryContext* queryContext,
        const std::string& storageId);

    void removeStorageBucketRelations(
        nx::sql::QueryContext* queryContext,
        const std::string& storageId);
};

//-------------------------------------------------------------------------------------------------
// StorageDaoFactory

using StorageDaoFactoryType = std::unique_ptr<AbstractStorageDao>();

class StorageDaoFactory:
    public nx::utils::BasicFactory<StorageDaoFactoryType>
{
    using base_type = nx::utils::BasicFactory<StorageDaoFactoryType>;

public:
    StorageDaoFactory();

    static StorageDaoFactory& instance();

private:
    std::unique_ptr<AbstractStorageDao> defaultFactoryFunction();
};

} // namespace dao
} // namespace model
} // namespace nx::cloud::storage::service