#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/network/cloud/storage/service/api/add_storage.h>
#include <nx/network/cloud/storage/service/api/bucket.h>
#include <nx/network/cloud/storage/service/api/result.h>
#include <nx/network/cloud/storage/service/api/storage.h>
#include <nx/network/cloud/storage/service/api/storage_credentials.h>
#include <nx/sql/query_context.h>
#include <nx/utils/basic_factory.h>

#include "bucket_manager.h"
#include "geo_ip_resolver.h"
#include "s3/data_usage_calculator.h"

namespace nx {

namespace network { class SocketAddress; }

namespace cloud {

namespace aws::sts { class ApiClient; }

namespace storage::service {

namespace conf { class Settings; }

namespace model {

namespace dao { class AbstractStorageDao; }
class Database;
class Model;

} // namespace model

namespace controller {

using GetStorageHandler = nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)>;

class StorageManager
{
public:
    StorageManager(
        const conf::Settings& settings,
        model::Model* model,
        BucketManager* bucketManager);
    ~StorageManager();

    void addStorage(
        const network::SocketAddress& clientEndpoint,
        const api::AddStorageRequest& request,
        GetStorageHandler handler);

    void readStorage(
        const std::string& storageId,
        GetStorageHandler handler);

    void removeStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler);

    void listCameras(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result, std::vector<std::string>)> handler);

    void getCredentials(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result, api::StorageCredentials)> handler);

private:
    void getStorage(
        const std::string& storageId,
        bool withDataUsage,
        GetStorageHandler handler);

    void getCredentialsForStorage(
        const std::string& storageId,
        api::Storage storage,
        nx::utils::MoveOnlyFunc<void(api::Result, api::StorageCredentials)> handler);

    nx::sql::DBResult addStorageAndSynchronize(
        nx::sql::QueryContext* queryContext,
        const api::Storage& storage);

    std::optional<api::Storage> removeStorageAndSynchronize(
        nx::sql::QueryContext* queryContext,
        const std::string& storageId);

    std::pair<api::Result, api::Bucket> findBucketByRegion(
        const std::vector<api::Bucket>& buckets,
        const std::string& request);

    std::pair<api::Result, api::Bucket> findClosestBucket(
        const std::vector<api::Bucket>& buckets,
        const network::SocketAddress& clientEndpoint);

    std::optional<nx::geo_ip::Location> resolveLocation(const std::string& ipAddress);

    void insertReceivedRecord(
        nx::sql::QueryContext* queryContext,
        const std::string& /*clusterId*/,
        clusterdb::engine::Command<api::Storage> command);

    void removeReceivedRecord(
        nx::sql::QueryContext* queryContext,
        const std::string& /*clusterId*/,
        clusterdb::engine::Command<std::string> command);

    void calculateDataUsage(
        api::Storage storage,
        nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)> handler);

    std::shared_ptr<s3::DataUsageCalculator> createDataUsageCalculator();
    void removeDataUsageCalculator(const std::shared_ptr<s3::DataUsageCalculator>& calculator);

    void initializeStsClient();

private:
    struct AddStorageContext
    {
        api::Result bucketLookupResult;
        std::optional<api::Storage> storage;

        void initializeStorage(
            const api::Bucket& bucket,
            const api::AddStorageRequest& request);
    };

private:
    const conf::Settings& m_settings;
    model::Database* m_database = nullptr;
    model::dao::AbstractStorageDao* m_storageDao = nullptr;
    BucketManager* m_bucketManager = nullptr;
    std::unique_ptr<nx::geo_ip::AbstractResolver> m_geoIpResolver;
    QnMutex m_mutex;
    std::set<std::shared_ptr<s3::DataUsageCalculator>> m_dataUsageCalculators;
    std::unique_ptr<aws::sts::ApiClient> m_stsClient;
};

} // namespace controller
} // namespace storage::service
} // namespace cloud
} // namespace nx
