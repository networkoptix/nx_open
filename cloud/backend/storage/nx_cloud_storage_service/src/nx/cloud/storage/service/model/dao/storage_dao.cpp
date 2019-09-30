#include "storage_dao.h"

#include <nx/clusterdb/engine/synchronization_engine.h>
#include <nx/cloud/storage/service/api/storage.h>
#include <nx/cloud/storage/service/api/system.h>

#include "nx/cloud/storage/service/settings.h"
#include "nx/cloud/storage/service/model/command.h"
#include "nx/cloud/storage/service/model/database.h"
#include "nx/cloud/storage/service/utils.h"

namespace nx::cloud::storage::service::model::dao {

using namespace std::placeholders;

namespace {

static constexpr char kId[] = "id";
static constexpr char kRegion[] = "region";
static constexpr char kTotalSpace[] = "total_space";
static constexpr char kUrl[] = "url";
static constexpr char kSystemId[] = "system_id";
static constexpr char kOwner[] = "owner";

static constexpr char kIdBinding[] = ":id";
static constexpr char kTotalSpaceBinding[] = ":total_space";
static constexpr char kRegionBinding[] = ":region";
static constexpr char kUrlBinding[] = ":url";
static constexpr char kStorageIdBinding[] = ":storage_id";
static constexpr char kBucketNameBinding[] = ":bucket_name";
static constexpr char kSystemIdBinding[] = ":system_id";
static constexpr char kOwnerBinding[] = ":owner";

static constexpr char kAddStorage[] = R"sql(

INSERT INTO storages (id, total_space, owner)
VALUES(:id, :total_space, :owner)

)sql";

static constexpr char kAddStorageBucketRelation[] = R"sql(

INSERT INTO storage_bucket_relation (url, storage_id, bucket_name, region)
VALUES(:url, :storage_id, :bucket_name, :region)

)sql";

static constexpr char kReadStorage[] = R"sql(

SELECT * FROM storages WHERE id=:id

)sql";

static constexpr char kFetchDevicesForStorage[] = R"sql(

SELECT url, region FROM storage_bucket_relation WHERE storage_id=:storage_id

)sql";

static constexpr char kRemoveStorage[] = R"sql(

DELETE FROM storages WHERE id=:id

)sql";

static constexpr char kRemoveStorageBucketRelations[] = R"sql(

DELETE FROM storage_bucket_relation WHERE storage_id=:storage_id

)sql";

static constexpr char kAddStorageSystemRelation[] = R"sql(

INSERT INTO storage_system_relation(storage_id, system_id)
VALUES(:storage_id, :system_id)

)sql";

static constexpr char kRemoveStorageSystemRelation[] = R"sql(

DELETE FROM storage_system_relation
WHERE storage_id=:storage_id AND system_id=:system_id

)sql";

static constexpr char kRemoveStorageSystemRelations[] = R"sql(

DELETE FROM storage_system_relation
WHERE storage_id=:storage_id

)sql";

static constexpr char kFetchSystemsForStorage[] = R"sql(

SELECT system_id FROM storage_system_relation
WHERE storage_id=:storage_id

)sql";

api::Storage toStorage(nx::sql::AbstractSqlQuery* query)
{
    api::Storage storage;
    storage.id = query->value(kId).toString().toStdString();
    storage.totalSpace = query->value(kTotalSpace).toInt();
    storage.owner = query->value(kOwner).toString().toStdString();
    return storage;
}

} // namespace

StorageDao::StorageDao(const conf::Database& settings, Database* db):
    m_clusterId(settings.synchronization.clusterId),
    m_db(db)
{
    registerCommandHandlers();
}

StorageDao::~StorageDao()
{
    unregisterCommandHandlers();
}

nx::sql::AbstractAsyncSqlQueryExecutor& StorageDao::queryExecutor()
{
    return m_db->queryExecutor();
}

void StorageDao::addStorage(
    nx::sql::QueryContext* queryContext,
    const api::Storage& storage)
{
    m_db->syncEngine().transactionLog()
        .saveDbOperationToLog<command::SaveStorage>(
            queryContext,
            m_clusterId,
            storage,
            std::bind(&StorageDao::addStorageToDb, this, _1, _2));
}

std::optional<api::Storage> StorageDao::readStorage(
    nx::sql::QueryContext* queryContext,
    const std::string& storageId)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kReadStorage);
    query->bindValue(kIdBinding, storageId);
    query->exec();

    if (!query->next())
        return std::nullopt;

    auto storage = toStorage(query.get());

    if (storage.id != storageId)
        return std::nullopt;

    storage.ioDevices = getStorageDevices(queryContext, storageId);
    storage.systems = getSystems(queryContext, storageId);

    return storage;
}

void StorageDao::removeStorage(
    nx::sql::QueryContext* queryContext,
    const std::string& storageId)
{
    return m_db->syncEngine().transactionLog()
        .saveDbOperationToLog<command::RemoveStorage>(
            queryContext,
            m_clusterId,
            storageId,
            std::bind(&StorageDao::removeStorageFromDb, this, _1, _2));
}

void StorageDao::addSystem(
    nx::sql::QueryContext* queryContext,
    const api::System& system)
{
    m_db->syncEngine().transactionLog()
        .saveDbOperationToLog<command::SaveSystem>(
            queryContext,
            m_clusterId,
            system,
            std::bind(&StorageDao::addSystemStorageRelation, this, _1, _2));
}

void StorageDao::removeSystem(
    nx::sql::QueryContext* queryContext,
    const api::System& system)
{
    m_db->syncEngine().transactionLog()
        .saveDbOperationToLog<command::RemoveSystem>(
            queryContext,
            m_clusterId,
            system,
            std::bind(&StorageDao::removeSystemStorageRelation, this, _1, _2));
}

void StorageDao::addStorageToDb(nx::sql::QueryContext* queryContext, const api::Storage& storage)
{
    if (storage.ioDevices.empty())
        throw nx::sql::Exception(nx::sql::DBResult::logicError, "ioDevices is empty");

    auto query = queryContext->connection()->createQuery();
    query->prepare(kAddStorage);
    query->bindValue(kIdBinding, storage.id);
    query->bindValue(kTotalSpaceBinding, storage.totalSpace);
    query->bindValue(kOwnerBinding, storage.owner);
    query->exec();

    addStorageBucketRelation(queryContext, storage);
}

void StorageDao::addStorageBucketRelation(
    nx::sql::QueryContext* queryContext,
    const api::Storage& storage)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kAddStorageBucketRelation);
    query->bindValue(kUrlBinding, storage.ioDevices.back().dataUrl.toString());
    query->bindValue(kStorageIdBinding, storage.id);
    query->bindValue(
        kBucketNameBinding,
        service::utils::bucketName(storage.ioDevices.back().dataUrl));
    query->bindValue(kRegionBinding, storage.ioDevices.back().region);
    query->exec();
}

void StorageDao::removeStorageFromDb(
    nx::sql::QueryContext* queryContext,
    const std::string& storageId)
{
    removeSystemStorageRelations(queryContext, storageId);
    removeStorageBucketRelations(queryContext, storageId);

    auto query = queryContext->connection()->createQuery();
    query->prepare(kRemoveStorage);
    query->bindValue(kIdBinding, storageId);
    query->exec();
}

void StorageDao::removeStorageBucketRelations(
    nx::sql::QueryContext* queryContext,
    const std::string& storageId)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kRemoveStorageBucketRelations);
    query->bindValue(kStorageIdBinding, storageId);
    query->exec();
}

void StorageDao::addSystemStorageRelation(
    nx::sql::QueryContext* queryContext,
    const api::System& system)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kAddStorageSystemRelation);
    query->bindValue(kStorageIdBinding, system.storageId);
    query->bindValue(kSystemIdBinding, system.id);
    query->exec();
}

void StorageDao::removeSystemStorageRelation(
    nx::sql::QueryContext* queryContext,
    const api::System& system)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kRemoveStorageSystemRelation);
    query->bindValue(kStorageIdBinding, system.storageId);
    query->bindValue(kSystemIdBinding, system.id);
    query->exec();
}


void StorageDao::removeSystemStorageRelations(
    nx::sql::QueryContext* queryContext,
    const std::string& storageId)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kRemoveStorageSystemRelations);
    query->bindValue(kStorageIdBinding, storageId);
    query->exec();
}

void StorageDao::registerCommandHandlers()
{
    m_db->syncEngine().incomingCommandDispatcher()
        .registerCommandHandler<command::SaveStorage>(
            std::bind(&StorageDao::addStorageToDb, this, _1, _3));

    m_db->syncEngine().incomingCommandDispatcher()
        .registerCommandHandler<command::RemoveStorage>(
            std::bind(&StorageDao::removeStorageFromDb, this, _1, _3));

    m_db->syncEngine().incomingCommandDispatcher()
        .registerCommandHandler<command::SaveSystem>(
            std::bind(&StorageDao::addSystemStorageRelation, this, _1, _3));

    m_db->syncEngine().incomingCommandDispatcher()
        .registerCommandHandler<command::RemoveSystem>(
            std::bind(&StorageDao::removeSystemStorageRelation, this, _1, _3));

}

void StorageDao::unregisterCommandHandlers()
{
    m_db->syncEngine().incomingCommandDispatcher().removeHandler<command::SaveStorage>();
    m_db->syncEngine().incomingCommandDispatcher().removeHandler<command::RemoveStorage>();
    m_db->syncEngine().incomingCommandDispatcher().removeHandler<command::SaveSystem>();
    m_db->syncEngine().incomingCommandDispatcher().removeHandler<command::RemoveSystem>();
}

std::vector<api::Device> StorageDao::getStorageDevices(
    nx::sql::QueryContext* queryContext,
    const std::string& storageId)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kFetchDevicesForStorage);
    query->bindValue(kStorageIdBinding, storageId);
    query->exec();

    std::vector<api::Device> devices;
    while (query->next())
    {
        devices.emplace_back(api::Device{});
        devices.back().region = query->value(kRegion).toString().toStdString();
        devices.back().dataUrl = query->value(kUrl).toString();
    }

    return devices;
}

std::vector<std::string> StorageDao::getSystems(
    nx::sql::QueryContext* queryContext,
    const std::string& storageId)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kFetchSystemsForStorage);
    query->bindValue(kStorageIdBinding, storageId);
    query->exec();

    std::vector<std::string> systems;
    while (query->next())
        systems.emplace_back(query->value(kSystemId).toString().toStdString());

    return systems;
}

//-------------------------------------------------------------------------------------------------
// StorageDaoFactory

StorageDaoFactory::StorageDaoFactory():
    base_type(std::bind(&StorageDaoFactory::defaultFactoryFunction, this, _1, _2))
{
}

StorageDaoFactory& StorageDaoFactory::instance()
{
    static StorageDaoFactory factory;
    return factory;
}

std::unique_ptr<AbstractStorageDao> StorageDaoFactory::defaultFactoryFunction(
    const conf::Database& settings,
    Database* db)
{
    return std::make_unique<StorageDao>(settings, db);
}

} // namespace nx::cloud::storage::service::model::dao