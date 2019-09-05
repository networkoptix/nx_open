#include "bucket_dao.h"

#include <nx/cloud/storage/service/api/bucket.h>
#include <nx/clusterdb/engine/synchronization_engine.h>

#include "nx/cloud/storage/service/settings.h"
#include "nx/cloud/storage/service/model/command.h"
#include "nx/cloud/storage/service/model/database.h"
#include "nx/cloud/storage/service/utils.h"

namespace nx::cloud::storage::service::model::dao {

using namespace std::placeholders;

namespace {

static constexpr char kName[] = "name";
static constexpr char kRegion[] = "region";
static constexpr char kStorageCount[] = "storage_count";

static constexpr char kNameBinding[] = ":name";
static constexpr char kRegionBinding[] = ":region";
static constexpr char kBucketNameBinding[] = ":bucket_name";

static constexpr char kAddBucket[] = R"sql(

INSERT INTO buckets(name, region)
VALUES(:name, :region)

)sql";

static constexpr char kListBuckets[] = R"sql(

SELECT * FROM buckets

)sql";

static constexpr char kRemoveBucket[] = R"sql(

DELETE FROM buckets WHERE name=:name

)sql";

static constexpr char kCountStorages[] = R"sql(

SELECT COUNT(storage_id) AS storage_count
FROM
    storage_bucket_relation
WHERE
    bucket_name=:bucket_name

)sql";

static api::Bucket toBucket(nx::sql::AbstractSqlQuery* query)
{
    return api::Bucket{
        query->value(kName).toByteArray().toStdString(),
        query->value(kRegion).toByteArray().toStdString()};
}

static std::vector<api::Bucket> toBuckets(nx::sql::AbstractSqlQuery* query)
{
    std::vector<api::Bucket> buckets;
    while (query->next())
        buckets.emplace_back(toBucket(query));
    return buckets;
}

} // namespace

BucketDao::BucketDao(const conf::Database& settings, Database* db):
    m_clusterId(settings.synchronization.clusterId),
    m_db(db)
{
    registerCommandHandlers();
}

BucketDao::~BucketDao()
{
    unregisterCommandHandlers();
}

nx::sql::AbstractAsyncSqlQueryExecutor& BucketDao::queryExecutor()
{
    return m_db->queryExecutor();
}

void BucketDao::addBucket(
    nx::sql::QueryContext* queryContext,
    const api::Bucket& bucket)
{
    m_db->syncEngine().transactionLog().saveDbOperationToLog<command::SaveBucket>(
            queryContext,
            m_clusterId,
            bucket,
            std::bind(&BucketDao::addBucketToDb, this, _1, _2));
}

std::vector<api::Bucket> BucketDao::fetchBuckets(nx::sql::QueryContext* queryContext)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kListBuckets);
    query->exec();

    auto buckets = toBuckets(query.get());
    for (auto& bucket : buckets)
        bucket.cloudStorageCount = getStorageCount(queryContext, bucket.name);

    return buckets;
}

void BucketDao::removeBucket(
    nx::sql::QueryContext* queryContext,
    const std::string& bucketName)
{
    m_db->syncEngine().transactionLog()
        .saveDbOperationToLog<command::RemoveBucket>(
            queryContext,
            m_clusterId,
            bucketName,
            std::bind(&BucketDao::removeBucketFromDb, this, _1, _2));
}

int BucketDao::getStorageCount(
    nx::sql::QueryContext* queryContext,
    const std::string& bucketName)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kCountStorages);
    query->bindValue(kBucketNameBinding, bucketName);
    query->exec();
    return query->next() ? query->value(kStorageCount).toInt() : -1;
}

void BucketDao::addBucketToDb(nx::sql::QueryContext* queryContext, const api::Bucket& bucket)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kAddBucket);
    query->bindValue(kNameBinding, bucket.name);
    query->bindValue(kRegionBinding, bucket.region);
    query->exec();
}

void BucketDao::removeBucketFromDb(nx::sql::QueryContext* queryContext, const std::string& bucketName)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kRemoveBucket);
    query->bindValue(kNameBinding, bucketName);
    query->exec();
}

void BucketDao::registerCommandHandlers()
{
    m_db->syncEngine().incomingCommandDispatcher()
        .registerCommandHandler<command::SaveBucket>(
            std::bind(&BucketDao::addBucketToDb, this, _1, _3));

    m_db->syncEngine().incomingCommandDispatcher()
        .registerCommandHandler<command::RemoveBucket>(
            std::bind(&BucketDao::removeBucketFromDb, this, _1, _3));
}

void BucketDao::unregisterCommandHandlers()
{
    m_db->syncEngine().incomingCommandDispatcher().removeHandler<command::SaveBucket>();
    m_db->syncEngine().incomingCommandDispatcher().removeHandler<command::RemoveBucket>();
}

//-------------------------------------------------------------------------------------------------
// BucketDaoFactory

BucketDaoFactory::BucketDaoFactory():
    base_type(std::bind(&BucketDaoFactory::defaultFactoryFunction, this, _1, _2))
{
}

BucketDaoFactory& BucketDaoFactory::instance()
{
    static BucketDaoFactory factory;
    return factory;
}

std::unique_ptr<AbstractBucketDao> BucketDaoFactory::defaultFactoryFunction(
    const conf::Database& settings,
    Database* db)
{
    return std::make_unique<BucketDao>(settings, db);
}

} // namespace nx::cloud::storage::service::model::dao
