#pragma once

#include <nx/network/cloud/storage/service/api/result.h>
#include <nx/network/cloud/storage/service/api/add_bucket.h>
#include <nx/network/cloud/storage/service/api/bucket.h>
#include <nx/sql/query_context.h>

#include "command.h"
#include "s3_permissions_tester.h"

namespace nx::cloud {

namespace aws { class ApiClient; }

namespace storage::service {

namespace conf { class Settings; }

namespace model {

namespace dao { class AbstractBucketDao; }
class Database;
class Model;

} // namespace model

namespace controller {

class BucketManager
{
public:
    BucketManager(const conf::Settings& settings, model::Model* model);

   void addBucket(
        const api::AddBucketRequest& request,
        nx::utils::MoveOnlyFunc<void(api::Result, api::Bucket)> handler);

    void listBuckets(
        nx::utils::MoveOnlyFunc<void(api::Result, api::Buckets)> handler);

    void removeBucket(
        const std::string& bucketName,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler);

    std::vector<api::Bucket> fetchBuckets(
        nx::sql::QueryContext* queryContext,
        bool withStorageCount = false);

private:
    nx::sql::DBResult addBucketInternal(
        nx::sql::QueryContext* queryContext,
        const api::Bucket& bucket);

    nx::sql::DBResult removeBucketInternal(
        nx::sql::QueryContext* queryContext,
        const std::string& bucketName);

    void insertReceivedRecord(
        nx::sql::QueryContext* queryContext,
        const std::string& /*clusterId*/,
        clusterdb::engine::Command<api::Bucket> command);

    void removeReceivedRecord(
        nx::sql::QueryContext* queryContext,
        const std::string& /*clusterId*/,
        clusterdb::engine::Command<std::string> command);

private:
    const conf::Settings& m_settings;
    model::Database* m_database = nullptr;
    model::dao::AbstractBucketDao* m_bucketDao = nullptr;
};

} // namespace controller
} // namespace storage::service
} // namespace nx::cloud

