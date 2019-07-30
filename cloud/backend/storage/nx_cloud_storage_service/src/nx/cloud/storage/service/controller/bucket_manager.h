#pragma once

#include <nx/network/cloud/storage/service/api/result.h>
#include <nx/network/cloud/storage/service/api/add_bucket.h>
#include <nx/network/cloud/storage/service/api/bucket.h>
#include <nx/sql/types.h>

#include "s3_permissions_tester.h"

namespace nx::sql { class QueryContext; }

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

private:
    nx::sql::DBResult addBucketInternal(
        nx::sql::QueryContext* queryContext,
        const api::Bucket& bucket);

    nx::sql::DBResult removeBucketInternal(
        nx::sql::QueryContext* queryContext,
        const std::string& bucketName);

private:
    const conf::Settings& m_settings;
    model::Database* m_database = nullptr;
    model::dao::AbstractBucketDao* m_bucketDao = nullptr;
};

} // namespace controller
} // namespace storage::service
} // namespace nx::cloud

