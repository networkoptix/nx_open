#include "bucket_dao.h"

#include <nx/sql/query_context.h>

#include "nx/cloud/storage/service/utils.h"

namespace nx::cloud::storage::service::model::dao {

namespace {

static constexpr char kName[] = "name";
static constexpr char kRegion[] = "region";
static constexpr char kStorageCount[] = "storage_count";

static constexpr char kNameBinding[] = ":name";
static constexpr char kRegionBinding[] = ":region";
static constexpr char kUrlBinding[] = ":url";

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

SELECT COUNT(id) AS storage_count FROM storages WHERE url=:url

)sql";

static api::Bucket toBucket(nx::sql::AbstractSqlQuery* query)
{
    return api::Bucket{
        query->value(kName).toByteArray().toStdString(),
        query->value(kRegion).toByteArray().toStdString()};
}

static std::vector<api::Bucket> toVector(nx::sql::AbstractSqlQuery* query)
{
    std::vector<api::Bucket> buckets;
    while (query->next())
        buckets.emplace_back(toBucket(query));
    return buckets;
}

static void updateStorageCount(nx::sql::QueryContext* queryContext, api::Bucket* outBucket)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kCountStorages);
    query->bindValue(kUrlBinding, service::utils::bucketUrl(outBucket->name).toString());
    query->exec();
    if (query->next())
        outBucket->cloudStorageCount = query->value(kStorageCount).toInt();
}

} // namespace

void BucketDao::addBucket(
    nx::sql::QueryContext* queryContext,
    const api::Bucket& bucket)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kAddBucket);
    query->bindValue(kNameBinding, bucket.name);
    query->bindValue(kRegionBinding, bucket.region);
    query->exec();
}

std::vector<api::Bucket> BucketDao::fetchBuckets(
    nx::sql::QueryContext* queryContext,
    bool withStorageCount)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kListBuckets);
    query->exec();

    auto buckets = toVector(query.get());
    if (withStorageCount)
    {
        for (auto& bucket : buckets)
            updateStorageCount(queryContext, &bucket);
    }

    return buckets;
}

void BucketDao::removeBucket(
    nx::sql::QueryContext* queryContext,
    const std::string& bucketName)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kRemoveBucket);
    query->bindValue(kNameBinding, bucketName);
    query->exec();
}

//-------------------------------------------------------------------------------------------------
// BucketDaoFactory

BucketDaoFactory::BucketDaoFactory():
    base_type(std::bind(&BucketDaoFactory::defaultFactoryFunction, this))
{
}

BucketDaoFactory& BucketDaoFactory::instance()
{
    static BucketDaoFactory factory;
    return factory;
}

std::unique_ptr<AbstractBucketDao> BucketDaoFactory::defaultFactoryFunction()
{
    return std::make_unique<BucketDao>();
}

} // namespace nx::cloud::storage::service::model::dao
