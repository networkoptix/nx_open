#pragma once

#include <nx/utils/move_only_func.h>

#include <nx/network/cloud/storage/service/api/add_storage.h>
#include <nx/network/cloud/storage/service/api/bucket.h>
#include <nx/network/cloud/storage/service/api/result.h>
#include <nx/network/cloud/storage/service/api/storage.h>
#include <nx/sql/query_context.h>
#include <nx/utils/basic_factory.h>

#include "bucket_manager.h"
#include "geo_ip_resolver.h"

namespace nx {

namespace network { class SocketAddress; }

namespace cloud::storage::service {

namespace conf { class Settings; }

namespace model {

namespace dao { class AbstractStorageDao; }
class Database;
class Model;

} // namespace model

namespace controller {

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
        nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)> handler);

    void readStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)> handler);

    void removeStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler);

    void listCameras(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result, std::vector<std::string>)> handler);

private:
    nx::sql::DBResult addStorageInternal(
        nx::sql::QueryContext* queryContext,
        const api::Storage& storage);

    std::optional<api::Storage> removeStorageInternal(
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

private:
    struct AddStorageContext
    {
        api::Result result;
        api::Storage storage;

        AddStorageContext(const api::AddStorageRequest& request);
    };

private:
    const conf::Settings& m_settings;
    model::Database* m_database = nullptr;
    model::dao::AbstractStorageDao* m_storageDao = nullptr;
    BucketManager* m_bucketManager = nullptr;
    std::unique_ptr<nx::geo_ip::AbstractResolver> m_geoIpResolver;

};

} // namespace controller
} // namespace cloud::storage::service
} // namespace nx
