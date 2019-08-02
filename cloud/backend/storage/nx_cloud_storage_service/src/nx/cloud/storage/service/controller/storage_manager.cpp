#include "storage_manager.h"

#include <nx/network/socket_common.h>

#include "nx/cloud/storage/service/settings.h"
#include "nx/cloud/storage/service/utils.h"
#include "nx/cloud/storage/service/model/model.h"
#include "nx/cloud/storage/service/model/database.h"
#include "nx/cloud/storage/service/model/dao/storage_dao.h"
#include "utils.h"

namespace nx::cloud::storage::service::controller {

namespace {

static constexpr char kStorageType[] = "awss3";

static api::Result badRequest(QString message)
{
    return api::Result(api::ResultCode::badRequest, message.toStdString());
}

static const std::map<std::string, nx::geo_ip::Geopoint> kAwsGeopoints = {
    {"us-east-2", {40, -83}},
    {"us-west-2", {44, -121}},
    {"us-west-1", {37, -121}},
    {"ap-east-1", {22.33, 114.17}},
    {"ap-south-1", {19, 72.9}},
    {"ap-northeast-3", {35, 135.97}},
    {"ap-northeast-2", {38, 126.97}},
    {"ap-southeast-1", {1.35, 103.81}},
    {"ap-southeast-2", {-33.86, 151}},
    {"ap-northeast-1", {36, 140}},
    {"ca-central-1", {45, -74}},
    {"cn-north-1", {40, 116.4}},
    {"cn-northwest-1", {37, 106}},
    {"eu-central-1", {50, 9}},
    {"eu-west-1", {53, -6}},
    {"eu-west-2", {51, 0}},
    {"eu-west-3", {48, 2}},
    {"eu-north-1", {59, 18}},
    {"sa-east-1", {-23.5, -46.6}},
    {"us-gov-west-1", {40, -104}},
    {"us-gov-east-1", {39, -78}}
};

} // namespace

StorageManager::AddStorageContext::AddStorageContext(const api::AddStorageRequest& request)
{
    storage.id = QnUuid::createUuid().toSimpleString().toStdString();
    storage.ioDevice.type = kStorageType;
    storage.totalSpace = request.totalSpace;
    storage.freeSpace = request.totalSpace;
}

//-------------------------------------------------------------------------------------------------
// StorageManager

StorageManager::StorageManager(
    const conf::Settings& settings,
    model::Model* model,
    BucketManager* bucketManager)
    :
    m_settings(settings),
    m_database(model->database()),
    m_storageDao(model->storageDao()),
    m_bucketManager(bucketManager),
    m_geoIpResolver(GeoIpResolverFactory::instance().create(settings))
{
}

StorageManager::~StorageManager() = default;

void StorageManager::addStorage(
    const network::SocketAddress& clientEndpoint,
    const api::AddStorageRequest& request,
    nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)> handler)
{
    if (request.totalSpace <= 0)
    {
        return handler(
            badRequest(lm("Invalid storage size: %1").arg(request.totalSpace)),
            api::Storage{});
    }

    auto addStorageContext = std::make_shared<AddStorageContext>(request);

    m_database->synchronizationEngine()->transactionLog().startDbTransaction(
        m_settings.database().synchronization.clusterId,
        [this, addStorageContext, request, clientEndpoint](auto queryContext)
        {
            auto buckets = m_bucketManager->fetchBuckets(queryContext);

            auto [result, bucket] = request.region.empty()
                ? findClosestBucket(buckets, clientEndpoint)
                : findBucketByRegion(buckets, request.region);

            addStorageContext->result = std::move(result);
            if (addStorageContext->result.resultCode != api::ResultCode::ok)
                return nx::sql::DBResult::ok;

            addStorageContext->storage.region = bucket.region;
            addStorageContext->storage.ioDevice.dataUrl = service::utils::bucketUrl(bucket.name);

            return addStorageInternal(queryContext, addStorageContext->storage);
        },
        [this, handler = std::move(handler), addStorageContext](auto dbResult)
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                api::Result error(utils::toResultCode(dbResult));
                error.error = lm("addStorage failed with sql error: %1")
                    .arg(toString(dbResult)).toStdString();
                NX_ERROR(this, error.error);
                return handler(std::move(error), api::Storage{});
            }

            if (addStorageContext->result.resultCode != api::ResultCode::ok)
                return handler(std::move(addStorageContext->result), api::Storage{});

            handler(api::Result(), std::move(addStorageContext->storage));
        });
}

void StorageManager::readStorage(
    const std::string& storageId,
    nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)> handler)
{
    if (storageId.empty())
        return handler(badRequest("Empty storageId"), api::Storage{});

    auto storage = std::make_shared<std::optional<api::Storage>>();

    m_database->synchronizationEngine()->transactionLog().startDbTransaction(
        m_settings.database().synchronization.clusterId,
        [this, storageId, storage](auto queryContext)
        {
            *storage = m_storageDao->readStorage(queryContext, storageId);
            return nx::sql::DBResult::ok;
        },
        [this, storageId, handler = std::move(handler), storage](auto dbResult) mutable
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                api::Result error(utils::toResultCode(dbResult));
                error.error = lm("readStorage(%1) failed with sql error: %2")
                    .args(storageId, toString(dbResult)).toStdString();
                NX_ERROR(this, error.error);
                return handler(std::move(error), api::Storage{});
            }

            if (!*storage)
                return handler(api::Result(api::ResultCode::notFound), api::Storage{});

            (*storage)->ioDevice.type = kStorageType;

            handler(api::Result(), std::move(**storage));
        });
}

void StorageManager::removeStorage(
    const std::string& storageId,
    nx::utils::MoveOnlyFunc<void(api::Result)> handler)
{
    if (storageId.empty())
        return handler(badRequest("Empty storageId"));

    auto storage = std::make_shared<std::optional<api::Storage>>();

    m_database->synchronizationEngine()->transactionLog().startDbTransaction(
        m_settings.database().synchronization.clusterId,
        [this, storageId, storage](auto queryContext)
        {
            *storage = removeStorageInternal(queryContext, storageId);
            return nx::sql::DBResult::ok;
        },
        [this, storageId, handler = std::move(handler), storage](auto dbResult)
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                api::Result error(utils::toResultCode(dbResult));
                error.error = lm("readStorage(%1) failed with sql error: %2")
                    .args(storageId, toString(dbResult)).toStdString();
                NX_ERROR(this, error.error);
                return handler(std::move(error));
            }

            if (!*storage)
                return handler(api::Result(api::ResultCode::notFound));

            handler(api::Result());
        });
}

void StorageManager::listCameras(
    const std::string& /*storageId*/,
    nx::utils::MoveOnlyFunc<void(api::Result, std::vector<std::string>)> handler)
{
    handler(api::Result{api::ResultCode::ok, "listCamerasOk"}, std::vector<std::string>());
}

nx::sql::DBResult StorageManager::addStorageInternal(
    nx::sql::QueryContext* queryContext,
    const api::Storage& storage)
{
    m_storageDao->addStorage(queryContext, storage);

    // TODO add sync commands

    return nx::sql::DBResult::ok;
}

std::optional<api::Storage> StorageManager::removeStorageInternal(
    nx::sql::QueryContext* queryContext,
    const std::string& storageId)
{
    auto storage = m_storageDao->readStorage(queryContext, storageId);
    if (!storage)
        return std::nullopt;

    m_storageDao->removeStorage(queryContext, storageId);

    // TODO add sync commands

    return storage;
}

std::pair<api::Result, api::Bucket> StorageManager::findBucketByRegion(
    const std::vector<api::Bucket>& buckets,
    const std::string& region)
{
    std::optional<api::Bucket> chosenBucket;
    for (const auto& bucket : buckets)
    {
        if (bucket.region == region)
        {
            chosenBucket = bucket;
            break;
        }
    }

    if (!chosenBucket)
    {
        api::Result error(api::ResultCode::notFound);
        error.error = lm("No bucket found in region: %1").arg(region).toStdString();
        NX_VERBOSE(this, error.error);
        return std::make_pair(std::move(error), api::Bucket{});
    }

    return std::make_pair(api::Result(), std::move(*chosenBucket));
}

std::pair<api::Result, api::Bucket> StorageManager::findClosestBucket(
    const std::vector<api::Bucket>& buckets,
    const network::SocketAddress& clientEndpoint)
{
    auto clientLocation = resolveLocation(clientEndpoint.address.toStdString());
    if (!clientLocation)
    {
        api::Result error(api::ResultCode::internalError);
        error.error = lm("Failed to resolve client IP %1 to a location")
            .arg(clientEndpoint.address.toString()).toStdString();
        NX_VERBOSE(this, error.error);
        return std::make_pair(std::move(error), api::Bucket{});
    }

    std::multimap<nx::geo_ip::Kilometers, api::Bucket> bucketsByDistance;
    for(const auto& bucket: buckets)
    {
        auto it = kAwsGeopoints.find(bucket.region);
        if (it == kAwsGeopoints.end())
        {
            NX_WARNING(this, "Unknown aws region '%1'", bucket.region);
            continue;
        }

        bucketsByDistance.emplace(clientLocation->geopoint.distanceTo(it->second), bucket);
    }

    if (bucketsByDistance.empty())
    {
        api::Result error(api::ResultCode::internalError);
        error.error = "No available buckets to use for adding storage";
        NX_ERROR(this, error.error);
        return std::make_pair(std::move(error), api::Bucket{});
    }

    return std::make_pair(api::Result(), bucketsByDistance.begin()->second);
}

std::optional<nx::geo_ip::Location> StorageManager::resolveLocation(const std::string& ipAddress)
{
    try
    {
        return m_geoIpResolver->resolve(ipAddress);
    }
    catch (const std::exception& e)
    {
        NX_VERBOSE(this, "Failed to resolve IP %1: %2", ipAddress, e.what());
    }

    return std::nullopt;
}

} // namespace nx::cloud::storage::service::controller
