#pragma once

#include <nx/sql/async_sql_query_executor.h>
#include <nx/utils/basic_factory.h>

namespace nx::cloud::storage::service {

namespace api { struct Bucket; }
namespace conf { struct Database; }

namespace model {

class Database;

namespace dao {

class AbstractBucketDao
{
public:
    virtual ~AbstractBucketDao() = default;

    virtual nx::sql::AbstractAsyncSqlQueryExecutor& queryExecutor() = 0;

    virtual void addBucket(
        nx::sql::QueryContext* queryContext,
        const api::Bucket& bucket) = 0;

    virtual std::vector<api::Bucket> fetchBuckets(nx::sql::QueryContext* queryContext) = 0;

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
    BucketDao(const conf::Database& settings, Database* db);
    ~BucketDao();

    virtual nx::sql::AbstractAsyncSqlQueryExecutor& queryExecutor() override;

    virtual void addBucket(
        nx::sql::QueryContext* queryContext,
        const api::Bucket& bucket) override;

    virtual std::vector<api::Bucket> fetchBuckets(nx::sql::QueryContext* queryContext) override;

    virtual void removeBucket(
        nx::sql::QueryContext* queryContext,
        const std::string& bucketName) override;

private:
    int getStorageCount(
        nx::sql::QueryContext* queryContext,
        const std::string& bucketName);

    void addBucketToDb(
        nx::sql::QueryContext* queryContext,
        const api::Bucket& bucket);

    void removeBucketFromDb(
        nx::sql::QueryContext* queryContext,
        const std::string& bucketName);

    void registerCommandHandlers();
    void unregisterCommandHandlers();

private:
    const std::string& m_clusterId;
    Database* m_db;
};

//-------------------------------------------------------------------------------------------------
// BucketDaoFactory

using BucketDaoFactoryType =
std::unique_ptr<AbstractBucketDao>(const conf::Database& settings, Database* db);

class BucketDaoFactory:
    public nx::utils::BasicFactory<BucketDaoFactoryType>
{
    using base_type = nx::utils::BasicFactory<BucketDaoFactoryType>;

public:
    BucketDaoFactory();

    static BucketDaoFactory& instance();

private:
    std::unique_ptr<AbstractBucketDao> defaultFactoryFunction(
        const conf::Database& settings,
        Database* db);
};

} // namespace dao
} // namespace model
} // namespace nx::cloud::storage::service
