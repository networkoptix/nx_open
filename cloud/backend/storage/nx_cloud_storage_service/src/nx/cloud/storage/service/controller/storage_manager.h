#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/cloud/storage/service/api/add_storage.h>
#include <nx/cloud/storage/service/api/add_system.h>
#include <nx/cloud/storage/service/api/bucket.h>
#include <nx/cloud/storage/service/api/result.h>
#include <nx/cloud/storage/service/api/storage.h>
#include <nx/cloud/storage/service/api/storage_credentials.h>
#include <nx/cloud/storage/service/api/system.h>
#include <nx/sql/query_context.h>
#include <nx/utils/counter.h>
#include <nx/utils/stree/resourcecontainer.h>

#include "bucket_manager.h"
#include "geo_ip_resolver.h"
#include "s3/data_usage_calculator.h"

namespace nx {

namespace clusterdb::engine { class Synchroni; }
namespace utils::stree { class ResourceContainer; }

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

class AccessManager;

using GetStorageHandler = nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)>;

class StorageManager
{
public:
    StorageManager(
        const conf::Settings& settings,
        model::Model* model,
        BucketManager* bucketManager);
    ~StorageManager();

    void stop();

    void addStorage(
        nx::utils::stree::ResourceContainer authInfo,
        network::SocketAddress clientEndpoint,
        api::AddStorageRequest request,
        GetStorageHandler handler);

    void readStorage(
        nx::utils::stree::ResourceContainer authInfo,
        std::string storageId,
        GetStorageHandler handler);

    void removeStorage(
        nx::utils::stree::ResourceContainer authInfo,
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler);

    void listCameras(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result, std::vector<std::string>)> handler);

    void getCredentials(
        nx::utils::stree::ResourceContainer authInfo,
        std::string storageId,
        nx::utils::MoveOnlyFunc<void(api::Result, api::StorageCredentials)> handler);

    void addSystem(
        nx::utils::stree::ResourceContainer authInfo,
        std::string storageId,
        api::AddSystemRequest request,
        nx::utils::MoveOnlyFunc<void(api::Result, api::System)> handler);

    void removeSystem(
        nx::utils::stree::ResourceContainer authInfo,
        std::string storageId,
        std::string systemId,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler);

private:
    struct AddStorageContext
    {
        api::Result bucketLookupResult;
        api::Storage storage;

        void initializeStorage(
            const std::string& accountOwner,
            const api::Bucket& bucket,
            const api::AddStorageRequest& request);
    };

    struct ReadStorageContext
    {
        std::string storageId;
        nx::utils::stree::ResourceContainer authInfo;
        bool withDataUsage = false;
        GetStorageHandler handler;
        api::Storage storage;
        bool found = false;

        ReadStorageContext(
            std::string storageId,
            nx::utils::stree::ResourceContainer authInfo,
            bool withDataUsage,
            GetStorageHandler handler);
    };

private:
    void getStorage(
        std::shared_ptr<ReadStorageContext> readStorageContext);

    void checkReadStorageAllowed(
        std::shared_ptr<ReadStorageContext> readStorageContext);

    void getCredentialsForStorage(
        api::Storage storage,
        nx::utils::MoveOnlyFunc<void(api::Result, api::StorageCredentials)> handler);

    std::pair<api::Result, api::Bucket> findBucketByRegion(
        const std::vector<api::Bucket>& buckets,
        const std::string& request);

    std::pair<api::Result, api::Bucket> findClosestBucket(
        const std::vector<api::Bucket>& buckets,
        const network::SocketAddress& clientEndpoint);

    std::optional<nx::geo_ip::Location> resolveLocation(const std::string& ipAddress);

    void calculateDataUsage(
        api::Storage storage,
        nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)> handler);

    std::shared_ptr<s3::DataUsageCalculator> createDataUsageCalculator();
    void removeDataUsageCalculator(const std::shared_ptr<s3::DataUsageCalculator>& calculator);

    std::pair<api::Result, std::string> validateAddStorageRequest(
        const nx::utils::stree::ResourceContainer& authInfo,
        const api::AddStorageRequest& request) const;

    api::Result prepareRemoveStorageResult(
        nx::utils::stree::ResourceContainer& authInfo,
        const std::string& storageId,
        nx::sql::DBResult dbResult,
        const std::optional<api::Storage>& storage) const;

    template<typename DbFunc, typename Handler>
    void modifySystemStorageRelation(
        const char* operation,
        nx::utils::stree::ResourceContainer authInfo,
        std::string storageId,
        std::string systemId,
        DbFunc dbFunc,
        Handler handler);

private:
    const conf::Settings& m_settings;
    const std::string& m_clusterId;
    model::dao::AbstractStorageDao* m_storageDao = nullptr;
    BucketManager* m_bucketManager = nullptr;
    std::unique_ptr<AccessManager> m_accessManager;
    std::unique_ptr<nx::geo_ip::AbstractResolver> m_geoIpResolver;
    QnMutex m_mutex;
    std::set<std::shared_ptr<s3::DataUsageCalculator>> m_dataUsageCalculators;
    std::unique_ptr<aws::sts::ApiClient> m_stsClient;
    nx::utils::Counter m_asyncCounter;
};

} // namespace controller
} // namespace storage::service
} // namespace cloud
} // namespace nx
