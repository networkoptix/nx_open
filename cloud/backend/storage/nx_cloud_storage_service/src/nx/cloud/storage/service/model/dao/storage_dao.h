#pragma once

#include <nx/sql/async_sql_query_executor.h>
#include <nx/utils/basic_factory.h>

namespace nx::cloud::storage::service {

namespace api {

struct Device;
struct Storage;
struct System;

} // namespace api

namespace conf { struct Database; }

namespace model {

class Database;

namespace dao {

class AbstractStorageDao
{
public:
    virtual ~AbstractStorageDao() = default;

    virtual nx::sql::AbstractAsyncSqlQueryExecutor& queryExecutor() = 0;

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
    StorageDao(const conf::Database& settings, Database* db);
    ~StorageDao();

    virtual nx::sql::AbstractAsyncSqlQueryExecutor& queryExecutor() override;

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
    void addStorageToDb(
        nx::sql::QueryContext* queryContext,
        const api::Storage& storage);

    void addStorageBucketRelation(
        nx::sql::QueryContext* queryContext,
        const api::Storage& storage);

    void removeStorageFromDb(
        nx::sql::QueryContext* queryContext,
        const std::string& storageId);

    void removeStorageBucketRelations(
        nx::sql::QueryContext* queryContext,
        const std::string& storageId);

    void addSystemStorageRelation(
        nx::sql::QueryContext* queryContext,
        const api::System& system);

    void removeSystemStorageRelation(
        nx::sql::QueryContext* queryContext,
        const api::System& system);

    void removeSystemStorageRelations(
        nx::sql::QueryContext* queryContext,
        const std::string& storageId);

    void registerCommandHandlers();
    void unregisterCommandHandlers();

    std::vector<api::Device> getStorageDevices(
        nx::sql::QueryContext* queryContext,
        const std::string& storageId);

    std::vector<std::string> getSystems(
        nx::sql::QueryContext* queryContext,
        const std::string& storageId);

private:
    const std::string& m_clusterId;
    Database* m_db;
};

//-------------------------------------------------------------------------------------------------
// StorageDaoFactory

using StorageDaoFactoryType =
std::unique_ptr<AbstractStorageDao>(const conf::Database& settings, Database* db);

class StorageDaoFactory:
    public nx::utils::BasicFactory<StorageDaoFactoryType>
{
    using base_type = nx::utils::BasicFactory<StorageDaoFactoryType>;

public:
    StorageDaoFactory();

    static StorageDaoFactory& instance();

private:
    std::unique_ptr<AbstractStorageDao> defaultFactoryFunction(
        const conf::Database& settings,
        Database* db);
};

} // namespace dao
} // namespace model
} // namespace nx::cloud::storage::service