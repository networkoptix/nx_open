#pragma once

#include <nx/cloud/storage/service/api/result.h>
#include <nx/cloud/storage/service/api/add_bucket.h>
#include <nx/cloud/storage/service/api/bucket.h>
#include <nx/sql/query_context.h>
#include <nx/utils/counter.h>

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

    void stop();

    void addBucket(const api::AddBucketRequest& request, AddBucketHandler handler);

    void listBuckets(
        nx::utils::MoveOnlyFunc<void(api::Result, api::Buckets)> handler);

    void removeBucket(
        const std::string& bucketName,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler);

    std::vector<api::Bucket> fetchBuckets(nx::sql::QueryContext* queryContext);

private:
    void addBucketInternal(
        std::string region,
        api::AddBucketRequest request,
        AddBucketHandler handler);

    std::shared_ptr<s3::PermissionsTester> createPermissionsTest(const std::string& bucketName);
    void removePermissionsTest(const std::shared_ptr<s3::PermissionsTester>& permissionsTester);

private:
    const conf::Settings& m_settings;
    const std::string& m_clusterId;
    model::dao::AbstractBucketDao* m_bucketDao = nullptr;
    QnMutex m_mutex;
    std::set<std::shared_ptr<s3::PermissionsTester>> m_permissionsTesters;
    nx::utils::Counter m_asyncCounter;
};

} // namespace controller
} // namespace storage::service
} // namespace nx::cloud

