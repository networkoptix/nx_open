#pragma once

#include <nx/utils/basic_factory.h>
#include <nx/network/cloud/storage/service/api/bucket.h>

namespace nx::sql { class QueryContext; }

namespace nx::cloud::storage::service::model::dao {

class AbstractBucketDao
{
public:
    virtual ~AbstractBucketDao() = default;

    virtual void addBucket(
        nx::sql::QueryContext* queryContext,
        const api::Bucket& bucket) = 0;

    virtual std::vector<api::Bucket> listBuckets(
        nx::sql::QueryContext* queryContext) = 0;

    virtual void removeBucket(
        nx::sql::QueryContext* queryContext,
        const std::string& bucketName) = 0;
};

//-------------------------------------------------------------------------------------------------
// BucketDao

class BucketDao:
    public AbstractBucketDao
{
public:
    virtual void addBucket(
        nx::sql::QueryContext* queryContext,
        const api::Bucket& bucket) override;

    virtual std::vector<api::Bucket> listBuckets(
        nx::sql::QueryContext* queryContext) override;

    virtual void removeBucket(
        nx::sql::QueryContext* queryContext,
        const std::string& bucketName) override;
};

//-------------------------------------------------------------------------------------------------
// BucketDaoFactory

using BucketDaoFactoryType = std::unique_ptr<AbstractBucketDao>();

class BucketDaoFactory:
    public nx::utils::BasicFactory<BucketDaoFactoryType>
{
    using base_type = nx::utils::BasicFactory<BucketDaoFactoryType>;

public:
    BucketDaoFactory();

    static BucketDaoFactory& instance();

private:
    std::unique_ptr<AbstractBucketDao> defaultFactoryFunction();
};


} // namespace nx::cloud::storage::service::model::dao