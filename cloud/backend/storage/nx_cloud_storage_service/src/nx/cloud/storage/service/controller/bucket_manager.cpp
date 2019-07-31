#include "bucket_manager.h"

#include <nx/cloud/aws/api_client.h>
#include <nx/clusterdb/engine/synchronization_engine.h>
#include <nx/network/url/url_builder.h>

#include "nx/cloud/storage/service/settings.h"
#include "nx/cloud/storage/service/model/model.h"
#include "nx/cloud/storage/service/model/database.h"
#include "nx/cloud/storage/service/model/dao/bucket_dao.h"
#include "nx/cloud/storage/service/utils.h"
#include "command.h"
#include "utils.h"

namespace nx::cloud::storage::service::controller {

BucketManager::BucketManager(const conf::Settings& settings, model::Model* model):
    m_settings(settings),
    m_database(model->database()),
    m_bucketDao(model->bucketDao())
{
    m_database->synchronizationEngine()->incomingCommandDispatcher().
        registerCommandHandler<command::SaveBucket>(
            [this](auto&& ... args)
            {
                insertReceivedRecord(std::forward<decltype(args)>(args)...);
                return nx::sql::DBResult::ok;
            });

    m_database->synchronizationEngine()->incomingCommandDispatcher().
        registerCommandHandler<command::RemoveBucket>(
            [this](auto&& ... args)
            {
                removeReceivedRecord(std::forward<decltype(args)>(args)...);
                return nx::sql::DBResult::ok;
            });
}

void BucketManager::addBucket(
    const api::AddBucketRequest& request,
    nx::utils::MoveOnlyFunc<void(api::Result, api::Bucket)> handler)
{
    if (request.name.empty())
    {
        return handler(
            api::Result(api::ResultCode::badRequest, "Empty bucket name"),
            api::Bucket{});
    }

    auto permissionsTester = std::make_unique<S3PermissionsTester>(
        m_settings.aws().credentials,
        service::utils::bucketUrl(request.name));

    permissionsTester->doTest(
        [this, permissionsTester = std::move(permissionsTester), request,
            handler = std::move(handler)](
                api::Result result,
                std::string region) mutable
        {
            if (result.resultCode != api::ResultCode::ok)
            {
                NX_ERROR(this, "S3 permissions test failed: %1, %2",
                    toString(result.resultCode), result.error);
                return handler(std::move(result), api::Bucket{});
            }

            auto bucket = std::make_shared<api::Bucket>();
            bucket->name = request.name;
            bucket->region = region;

            m_database->synchronizationEngine()->transactionLog().startDbTransaction(
                m_settings.database().synchronization.clusterId,
                [this, bucket](nx::sql::QueryContext* queryContext)
                {
                    return addBucketInternal(queryContext, *bucket);
                },
                [this, handler = std::move(handler), bucket](
                    nx::sql::DBResult dbResult)
                {
                    if (dbResult != nx::sql::DBResult::ok)
                    {
                        api::Result error(utils::toResultCode(dbResult));
                        error.error =
                            std::string("addBucket failed with sql error: ") + toString(dbResult);
                        NX_VERBOSE(this, error.error);
                        return handler(std::move(error), api::Bucket{});
                    }

                    handler(api::Result(), std::move(*bucket));
                });
        });
}

void BucketManager::listBuckets(
    nx::utils::MoveOnlyFunc<void(api::Result, api::Buckets)> handler)
{
    auto buckets = std::make_shared<api::Buckets>();

    m_database->synchronizationEngine()->transactionLog().startDbTransaction(
        m_settings.database().synchronization.clusterId,
        [this, buckets](nx::sql::QueryContext* queryContext)
        {
            buckets->buckets = m_bucketDao->listBuckets(queryContext);
            return nx::sql::DBResult::ok;
        },
        [this, handler = std::move(handler), buckets](nx::sql::DBResult dbResult)
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                api::Result error(utils::toResultCode(dbResult));
                error.error =
                    std::string("listBuckets failed with sql error: ") + toString(dbResult);
                NX_VERBOSE(this, error.error);
                return handler(std::move(error), api::Buckets{});
            }

            handler(api::Result(), std::move(*buckets));
        });
}

void BucketManager::removeBucket(
    const std::string& bucketName,
    nx::utils::MoveOnlyFunc<void(api::Result)> handler)
{
    m_database->synchronizationEngine()->transactionLog().startDbTransaction(
        m_settings.database().synchronization.clusterId,
        [this, bucketName](nx::sql::QueryContext* queryContext)
        {
            return removeBucketInternal(queryContext, bucketName);
        },
        [this, handler = std::move(handler), bucketName](nx::sql::DBResult dbResult)
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                api::Result error(utils::toResultCode(dbResult));
                error.error =
                    lm("removeBucket(%1) failed with sql error: %2")
                        .args(bucketName, toString(dbResult)).toStdString();
                NX_VERBOSE(this, error.error);
                return handler(std::move(error));
            }

            handler(api::Result());
        });
}

nx::sql::DBResult BucketManager::addBucketInternal(
    nx::sql::QueryContext* queryContext,
    const api::Bucket& bucket)
{
    m_bucketDao->addBucket(queryContext, bucket);

    m_database->synchronizationEngine()->transactionLog()
        .generateTransactionAndSaveToLog<command::SaveBucket>(
            queryContext,
            m_settings.database().synchronization.clusterId,
            Bucket{bucket.name, bucket.region});

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult BucketManager::removeBucketInternal(
    nx::sql::QueryContext* queryContext,
    const std::string& bucketName)
{
    m_bucketDao->removeBucket(queryContext, bucketName);

    m_database->synchronizationEngine()->transactionLog()
        .generateTransactionAndSaveToLog<command::RemoveBucket>(
            queryContext,
            m_settings.database().synchronization.clusterId,
            bucketName);

    return nx::sql::DBResult::ok;
}

void BucketManager::insertReceivedRecord(
    nx::sql::QueryContext* queryContext,
    const std::string& /*clusterId*/,
    clusterdb::engine::Command<Bucket> command)
{
    m_bucketDao->addBucket(
        queryContext,
        api::Bucket{
            command.params.name,
            command.params.region});
}

void BucketManager::removeReceivedRecord(
    nx::sql::QueryContext* queryContext,
    const std::string& /*clusterId*/,
    clusterdb::engine::Command<std::string> command)
{
    m_bucketDao->removeBucket(queryContext, command.params);
}

} // namespace nx::cloud::storage::service::controller
