#include "bucket_dao.h"

#include <nx/sql/query_context.h>

#include "nx/cloud/storage/service/utils.h"

namespace nx::cloud::storage::service::model::dao {

namespace {

static constexpr char kName[] = "name";
static constexpr char kRegion[] = "region";
static constexpr char kCloudStorageCount[] = "cloud_storage_count";

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

static constexpr char kCountStorages[] = R"sql(

SELECT COUNT(id) AS cloud_storage_count FROM storages WHERE url=:url

)sql";

static constexpr char kRemoveBucket[] = R"sql(

DELETE FROM buckets WHERE name=:name

)sql";

static std::vector<api::Bucket> toBucketVector(nx::sql::AbstractSqlQuery* query)
{
    std::vector<api::Bucket> buckets;
    while (query->next())
    {
        api::Bucket bucket;
        bucket.name = query->value(kName).toByteArray().toStdString();
        bucket.region = query->value(kRegion).toByteArray().toStdString();
        buckets.emplace_back(std::move(bucket));
    }
    return buckets;
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

std::vector<api::Bucket> BucketDao::listBuckets(
    nx::sql::QueryContext* queryContext)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(kListBuckets);
    query->exec();

    // TODO find a way to get cloud storage count without a query for each bucket.
    auto buckets = toBucketVector(query.get());
    for (auto& bucket: buckets)
    {
        query = queryContext->connection()->createQuery();
        query->prepare(kCountStorages);
        query->bindValue(kUrlBinding, utils::bucketUrl(bucket.name).toString());
        query->exec();
        if(query->next()) //< Not sure why this is necessary
            bucket.cloudStorageCount = query->value(kCloudStorageCount).toInt();
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
