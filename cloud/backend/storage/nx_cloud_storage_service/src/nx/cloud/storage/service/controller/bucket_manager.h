#pragma once

#include <nx/cloud/storage/service/api/result.h>
#include <nx/cloud/storage/service/api/add_bucket.h>
#include <nx/cloud/storage/service/api/bucket.h>
#include <nx/sql/query_context.h>

#include "command.h"
#include "s3/permissions_tester.h"

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

using AddBucketHandler = nx::utils::MoveOnlyFunc<void(api::Result, api::Bucket)>;

class BucketManager
{
public:
    BucketManager(const conf::Settings& settings, model::Model* model);
    ~BucketManager();

   void addBucket(const api::AddBucketRequest& request, AddBucketHandler handler);

    void listBuckets(
        nx::utils::MoveOnlyFunc<void(api::Result, api::Buckets)> handler);

    void removeBucket(
        const std::string& bucketName,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler);

    std::vector<api::Bucket> fetchBuckets(
        nx::sql::QueryContext* queryContext,
        bool withStorageCount = false);

private:
    void addBucketToDb(
        std::string region,
        api::AddBucketRequest request,
        AddBucketHandler handler);

    nx::sql::DBResult addBucketAndSynchronize(
        nx::sql::QueryContext* queryContext,
        const api::Bucket& bucket);

    nx::sql::DBResult removeBucketAndSynchronize(
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

    void onPermissionsTestDone(
        api::Result result,
        std::string region,
        api::AddBucketRequest request,
        std::shared_ptr<s3::PermissionsTester> permissionsTester,
        AddBucketHandler handler);

    std::shared_ptr<s3::PermissionsTester> createPermissionsTest(const std::string& bucketName);
    void removePermissionsTest(const std::shared_ptr<s3::PermissionsTester>& permissionsTester);

private:
    const conf::Settings& m_settings;
    model::Database* m_database = nullptr;
    model::dao::AbstractBucketDao* m_bucketDao = nullptr;
    QnMutex m_mutex;
    std::set<std::shared_ptr<s3::PermissionsTester>> m_permissionsTesters;
};

} // namespace controller
} // namespace storage::service
} // namespace nx::cloud

