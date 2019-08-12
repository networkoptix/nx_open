#include "storage_dao.h"

#include <nx/sql/query_context.h>

#include "nx/cloud/storage/service/model/database.h"
#include "nx/cloud/storage/service/utils.h"

namespace nx::cloud::storage::service::model::dao {

namespace {

static constexpr char kId[] = "id";
static constexpr char kRegion[] = "region";
static constexpr char kTotalSpace[] = "total_space";
static constexpr char kUrl[] = "url";

static constexpr char kIdBinding[] = ":id";
static constexpr char kTotalSpaceBinding[] = ":total_space";
static constexpr char kRegionBinding[] = ":region";
static constexpr char kUrlBinding[] = ":url";
static constexpr char kStorageIdBinding[] = ":storage_id";
static constexpr char kBucketNameBinding[] = ":bucket_name";

static constexpr char kAddStorage[] = R"sql(

INSERT INTO storages (id, total_space)
VALUES(:id, :total_space)

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

api::Storage toStorage(nx::sql::AbstractSqlQuery* query)
{
    api::Storage storage;
    storage.id = query->value(kId).toByteArray().toStdString();
    storage.totalSpace = query->value(kTotalSpace).toInt();
    return storage;
}

} // namespace

void StorageDao::addStorage(
    nx::sql::QueryContext* queryContext,
    const api::Storage& storage)
{
    if (storage.ioDevices.empty())
        throw nx::sql::Exception(nx::sql::DBResult::logicError, "ioDevices is empty");

    auto query = queryContext->connection()->createQuery();
    query->prepare(kAddStorage);
    query->bindValue(kIdBinding, storage.id);
    query->bindValue(kTotalSpaceBinding, storage.totalSpace);
    query->exec();

    addStorageBucketRelation(queryContext, storage);
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

    return storage;
}

void StorageDao::removeStorage(
    nx::sql::QueryContext* queryContext,
    const std::string& storageId)
{
    removeStorageBucketRelations(queryContext, storageId);

    auto query = queryContext->connection()->createQuery();
    query->prepare(kRemoveStorage);
    query->bindValue(kIdBinding, storageId);
    query->exec();
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

void StorageDao::removeStorageBucketRelations(
    nx::sql::QueryContext* queryContext,
    const std::string& storageId)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kRemoveStorageBucketRelations);
    query->bindValue(kStorageIdBinding, storageId);
    query->exec();
}

//-------------------------------------------------------------------------------------------------
// StorageDaoFactory

StorageDaoFactory::StorageDaoFactory():
    base_type(std::bind(&StorageDaoFactory::defaultFactoryFunction, this))
{
}

StorageDaoFactory& StorageDaoFactory::instance()
{
    static StorageDaoFactory factory;
    return factory;
}

std::unique_ptr<AbstractStorageDao> StorageDaoFactory::defaultFactoryFunction()
{
    return std::make_unique<StorageDao>();
}

} // namespace nx::cloud::storage::service::model::dao