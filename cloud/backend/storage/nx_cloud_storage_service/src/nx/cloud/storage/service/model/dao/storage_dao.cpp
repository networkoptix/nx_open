#include "storage_dao.h"

#include <nx/sql/query_context.h>

#include "nx/cloud/storage/service/model/database.h"

namespace nx::cloud::storage::service::model::dao {

namespace {

// TODO should this type by put into the database?
static constexpr char kStorageType[] = "awss3";

static constexpr char kId[] = "id";
static constexpr char kRegion[] = "region";
static constexpr char kTotalSpace[] = "total_space";
static constexpr char kUrl[] = "url";

static constexpr char kIdBinding[] = ":id";
static constexpr char kTotalSpaceBinding[] = ":total_space";
static constexpr char kRegionBinding[] = ":region";
static constexpr char kUrlBinding[] = ":url";

static constexpr char kAddStorage[] = R"sql(

INSERT INTO storages (id, total_space, region, url)
VALUES(:id, :total_space, :region, :url)

)sql";

static constexpr char kReadStorage[] = R"sql(

SELECT * FROM storages WHERE id=:id

)sql";

static constexpr char kRemoveStorage[] = R"sql(

DELETE FROM storages WHERE id=:id

)sql";

api::Storage toStorage(nx::sql::AbstractSqlQuery* query)
{
    api::Storage storage;
    storage.id = query->value(kId).toByteArray().toStdString();
    storage.totalSpace = query->value(kTotalSpace).toInt();
    storage.region = query->value(kRegion).toByteArray().toStdString();
    storage.ioDevice.dataUrl = query->value(kUrl).toString();
    storage.ioDevice.type = kStorageType; //< TODO put this in the DB?
    return storage;
}

} // namespace

void StorageDao::addStorage(
    nx::sql::QueryContext* queryContext,
    const api::Storage& storage)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kAddStorage);
    query->bindValue(kIdBinding, storage.id);
    query->bindValue(kRegionBinding, storage.region);
    query->bindValue(kTotalSpaceBinding, storage.totalSpace);
    query->bindValue(kUrlBinding, storage.ioDevice.dataUrl.toString());
    query->exec();
}

api::Storage StorageDao::readStorage(
    nx::sql::QueryContext* queryContext,
    const std::string& storageId)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kReadStorage);
    query->bindValue(kIdBinding, storageId);
    query->exec();

    auto storage = toStorage(query.get());
    // TODO request actual storage free space from S3, probably in controller code
    storage.freeSpace = storage.totalSpace;

    return storage;
}

void StorageDao::removeStorage(
    nx::sql::QueryContext* queryContext,
    const std::string& storageId)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kRemoveStorage);
    query->bindValue(kIdBinding, storageId);
    query->exec();
}

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