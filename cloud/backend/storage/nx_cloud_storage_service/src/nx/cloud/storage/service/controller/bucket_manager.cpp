#include "bucket_manager.h"

#include <nx/cloud/aws/s3/api_client.h>
#include <nx/clusterdb/engine/synchronization_engine.h>
#include <nx/network/url/url_builder.h>

#include "nx/cloud/storage/service/settings.h"
#include "nx/cloud/storage/service/model/command.h"
#include "nx/cloud/storage/service/model/model.h"
#include "nx/cloud/storage/service/model/database.h"
#include "nx/cloud/storage/service/model/dao/bucket_dao.h"
#include "nx/cloud/storage/service/utils.h"
#include "utils.h"

namespace nx::cloud::storage::service::controller {

using namespace std::placeholders;
using namespace model::dao;

BucketManager::BucketManager(const conf::Settings& settings, model::Model* model):
    m_settings(settings),
    m_clusterId(settings.database().synchronization.clusterId),
    m_bucketDao(&model->bucketDao())
{
}

BucketManager::~BucketManager()
{
    m_asyncCounter.wait();

    QnMutexLocker lock(&m_mutex);
    auto permissionsTesters = std::move(m_permissionsTesters);
    lock.unlock();

    for (auto& permissionsTester: permissionsTesters)
        permissionsTester->pleaseStopSync();
}

void BucketManager::addBucket(const api::AddBucketRequest& request, AddBucketHandler handler)
{
    using namespace std::placeholders;

    if (request.name.empty())
        return handler(utils::badRequest("Empty bucket name"), api::Bucket());

    auto permissionsTester = createPermissionsTest(request.name);

    permissionsTester->doTest(
        [this, request, handler = std::move(handler),
            permissionsTester](
                auto result,
                auto region) mutable
        {
            removePermissionsTest(permissionsTester);

            if (result.resultCode != api::ResultCode::ok)
            {
                NX_INFO(this, "S3 permissions test failed: %1, %2",
                    toString(result.resultCode), result.error);
                return handler(std::move(result), api::Bucket());
            }

            addBucketToDb(std::move(region), std::move(request), std::move(handler));
        });
}

void BucketManager::listBuckets(
    nx::utils::MoveOnlyFunc<void(api::Result, api::Buckets)> handler)
{
    auto buckets = std::make_shared<api::Buckets>();

    m_bucketDao->queryExecutor().executeSelect(
        [this, guard = m_asyncCounter.getScopedIncrement(), buckets](auto queryContext)
        {
            buckets->buckets = fetchBuckets(queryContext, true /*withStorageCount*/);
            return nx::sql::DBResult::ok;
        },
        [this, guard = m_asyncCounter.getScopedIncrement(),
            handler = std::move(handler), buckets](
                auto dbResult)
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                api::Result error(utils::toResultCode(dbResult));
                error.error = lm("listBuckets failed with sql error: %1")
                    .arg(toString(dbResult)).toStdString();
                NX_VERBOSE(this, error.error);
                return handler(std::move(error), api::Buckets());
            }

            handler(api::Result(), std::move(*buckets));
        });
}

void BucketManager::removeBucket(
    const std::string& bucketName,
    nx::utils::MoveOnlyFunc<void(api::Result)> handler)
{
    m_bucketDao->queryExecutor().executeUpdate(
        [this, guard = m_asyncCounter.getScopedIncrement(), bucketName](auto queryContext)
        {
            return m_bucketDao->removeBucket(queryContext, bucketName);
        },
        [this, guard = m_asyncCounter.getScopedIncrement(), handler = std::move(handler),
            bucketName](
                auto dbResult)
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                api::Result error(utils::toResultCode(dbResult));
                error.error = lm("removeBucket(%1) failed with sql error: %2")
                    .args(bucketName, toString(dbResult)).toStdString();
                NX_VERBOSE(this, error.error);
                return handler(std::move(error));
            }

            handler(api::Result());
        });
}

std::vector<api::Bucket> BucketManager::fetchBuckets(
    nx::sql::QueryContext* queryContext,
    bool withStorageCount)
{
    return m_bucketDao->fetchBuckets(queryContext, withStorageCount);
}

void BucketManager::addBucketToDb(
    std::string region,
    api::AddBucketRequest request,
    AddBucketHandler handler)
{
    auto bucket = std::make_shared<api::Bucket>();
    bucket->name = std::move(request.name);
    bucket->region = std::move(region);
    bucket->cloudStorageCount = 0;

    m_bucketDao->queryExecutor().executeUpdate(
        [this, guard = m_asyncCounter.getScopedIncrement(), bucket](auto queryContext)
        {
            return m_bucketDao->addBucket(queryContext, *bucket);
        },
        [this, guard = m_asyncCounter.getScopedIncrement(), handler = std::move(handler),
            bucket](
                auto dbResult)
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                api::Result error(utils::toResultCode(dbResult));
                error.error =
                    std::string("addBucket failed with sql error: ") + toString(dbResult);
                NX_VERBOSE(this, error.error);
                return handler(std::move(error), api::Bucket());
            }

            handler(api::Result(), std::move(*bucket));
        });
}

std::shared_ptr<s3::PermissionsTester> BucketManager::createPermissionsTest(
    const std::string& bucketName)
{
    auto permissionsTester =
        std::make_shared<s3::PermissionsTester>(
            m_settings.aws().credentials,
            service::utils::bucketUrl(bucketName));

    {
        QnMutexLocker lock(&m_mutex);
        m_permissionsTesters.emplace(permissionsTester);
    }

    return permissionsTester;
}

void BucketManager::removePermissionsTest(
    const std::shared_ptr<s3::PermissionsTester>& permissionsTester)
{
    QnMutexLocker lock(&m_mutex);
    m_permissionsTesters.erase(permissionsTester);
}

} // namespace nx::cloud::storage::service::controller
