#include "storage_manager.h"

#include <nx/network/socket_common.h>
#include <nx/network/url/url_builder.h>
#include <nx/cloud/aws/sts/api_client.h>
#include <nx/utils/stree/resourcecontainer.h>

#include "nx/cloud/storage/service/settings.h"
#include "nx/cloud/storage/service/utils.h"
#include "nx/cloud/storage/service/model/command.h"
#include "nx/cloud/storage/service/model/model.h"
#include "nx/cloud/storage/service/model/database.h"
#include "nx/cloud/storage/service/model/dao/storage_dao.h"
#include "access_manager.h"
#include "utils.h"

namespace nx::cloud::storage::service::controller {

using namespace std::placeholders;
using namespace model::dao;
using namespace nx::cloud::storage::service::api;


namespace {

// TODO should this be a setting? It is the aws global sts endpoint, but there are regional ones...
static constexpr char kStsUrl[] = "https://sts.amazonaws.com";
static constexpr char kStorageType[] = "awss3";

static const std::map<std::string, nx::geo_ip::Geopoint> kAwsGeopoints = {
    {"us-east-1", {39, -78}},
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

// TODO attempt to format this string properly when sts::ApiClient works
// %1 is an array of arns with the form: "arn:aws:s3:::examplebucket/folder".
// NOTE: Because storages can be merged, access must be granted to multiple buckets/folders.
// There is no way know the specific bucket/folder will be be written to, access must be granted
// to all of them.
static constexpr char kStorageAccessPolicyTemplate[] =
    R"json({"Version":"2012-10-17","Statement":[{"Sid":"S3ReadWriteAccess%1","Effect":"Allow","Action":["s3:GetObject","s3:PutObject","s3:DeleteObject"],"Resource":[%2]}]})json";

static std::string buildStorageAccessPolicy(const Storage& storage)
{
    static constexpr char kArnTemplate[] = "arn:aws:s3:::%1/%2";

    QString arns;
    for (const auto& ioDevice : storage.ioDevices)
    {
        QString arn = lm(kArnTemplate).args(
            service::utils::bucketName(ioDevice.dataUrl),
            service::utils::storageFolder(ioDevice.dataUrl));

        arns += "\"" + arn + "\",";        //< "arn:aws:s3:::examplebucket/folder",
        arns += "\"" + arn + "/*" + "\","; //< "arn:aws:s3:::examplebucket/folder/*",
    }

    if (arns.endsWith(','))
        arns.truncate(arns.size() - 1);

    return lm(kStorageAccessPolicyTemplate)
        .args(QnUuid::createUuid().toSimpleString(), arns).toStdString();
}

} // namespace

void StorageManager::AddStorageContext::initializeStorage(
    const std::string& accountOwner,
    const Bucket& bucket,
    const AddStorageRequest& request)
{
    storage.id = QnUuid::createUuid().toSimpleString().toStdString();
    storage.totalSpace = request.totalSpace;
    storage.freeSpace = request.totalSpace;
    storage.ioDevices.emplace_back(Device());
    storage.ioDevices.back().type = kStorageType;
    storage.ioDevices.back().dataUrl =
        network::url::Builder(service::utils::bucketUrl(bucket.name)).setPath(storage.id);
    storage.ioDevices.back().region = bucket.region;
    storage.owner = accountOwner;
}

StorageManager::ReadStorageContext::ReadStorageContext(
    std::string storageId,
    nx::utils::stree::ResourceContainer authInfo,
    bool withDataUsage,
    GetStorageHandler handler)
    :
    storageId(std::move(storageId)),
    authInfo(std::move(authInfo)),
    withDataUsage(withDataUsage),
    handler(std::move(handler))
{
}

//-------------------------------------------------------------------------------------------------
// StorageManager

StorageManager::StorageManager(
    const conf::Settings& settings,
    model::Model* model,
    BucketManager* bucketManager)
    :
    m_settings(settings),
    m_clusterId(settings.database().synchronization.clusterId),
    m_storageDao(&model->storageDao()),
    m_bucketManager(bucketManager),
    m_accessManager(std::make_unique<AccessManager>(settings.cloudDb())),
    m_geoIpResolver(GeoIpResolverFactory::instance().create(settings)),
    m_stsClient(
        std::make_unique<aws::sts::ApiClient>(
            utils::kDefaultBucketRegion,
            kStsUrl, //<TODO decide how to handle url, global or regional
            m_settings.aws().credentials))
{
}

StorageManager::~StorageManager()
{
    m_stsClient->pleaseStopSync();
    m_asyncCounter.wait();

    QnMutexLocker lock(&m_mutex);
    auto dataUsageCalculators = std::move(m_dataUsageCalculators);
    lock.unlock();

    for (auto& calculator: m_dataUsageCalculators)
        calculator->pleaseStopSync();
}

void StorageManager::addStorage(
    nx::utils::stree::ResourceContainer authInfo,
    network::SocketAddress clientEndpoint,
    AddStorageRequest request,
    GetStorageHandler handler)
{
    if (request.totalSpace <= 0)
    {
        return handler(
            Result(
                ResultCode::badRequest,
                lm("Invalid storage size: %1").arg(request.totalSpace).toStdString()),
            Storage());
    }

    NX_VERBOSE(this, "addStorage: request.region: %1, clientEndpoint: %2",
        request.region, clientEndpoint);

    auto [allowed, accountEmail] = m_accessManager->addStorageAllowed(authInfo);
    if (!allowed)
    {
        NX_VERBOSE(this, "addStorage failed: user %1 is unauthorized", accountEmail);
        return handler(ResultCode::unauthorized, Storage());
    }

    auto addStorageContext = std::make_shared<AddStorageContext>();

    m_storageDao->queryExecutor().executeUpdate(
        [this, guard = m_asyncCounter.getScopedIncrement(), addStorageContext, request,
            clientEndpoint, accountEmail = std::move(accountEmail)](
                 auto queryContext)
        {
            auto buckets = m_bucketManager->fetchBuckets(queryContext);

            auto [result, bucket] = request.region.empty()
                ? findClosestBucket(buckets, clientEndpoint)
                : findBucketByRegion(buckets, request.region);

            addStorageContext->bucketLookupResult = std::move(result);
            if (addStorageContext->bucketLookupResult.resultCode != ResultCode::ok)
                return nx::sql::DBResult::ok;

            addStorageContext->initializeStorage(accountEmail, bucket, request);

            return m_storageDao->addStorage(queryContext, addStorageContext->storage);
        },
        [this, guard = m_asyncCounter.getScopedIncrement(), handler = std::move(handler),
            addStorageContext](
            auto dbResult)
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                Result error(
                    utils::toResultCode(dbResult),
                    lm("addStorage failed with sql error: %1")
                        .arg(toString(dbResult)).toStdString());
                NX_ERROR(this, error.error);
                return handler(std::move(error), Storage());
            }

            if (addStorageContext->bucketLookupResult.resultCode != ResultCode::ok)
            {
                NX_VERBOSE(this, "addBucket failed during bucket lookup: %1, %2",
                    addStorageContext->bucketLookupResult.resultCode,
                    addStorageContext->bucketLookupResult.error);
                return handler(std::move(addStorageContext->bucketLookupResult), Storage());
            }

            handler(Result(), std::move(addStorageContext->storage));
        });
}

void StorageManager::readStorage(
    nx::utils::stree::ResourceContainer authInfo,
    std::string storageId,
    GetStorageHandler handler)
{
    auto readStorageContext =
        std::make_shared<StorageManager::ReadStorageContext>(
            std::move(storageId),
            std::move(authInfo),
            true /*withDataUsage*/,
            std::move(handler));
    getStorage(std::move(readStorageContext));
}

void StorageManager::removeStorage(
    nx::utils::stree::ResourceContainer authInfo,
    const std::string& storageId,
    nx::utils::MoveOnlyFunc<void(Result)> handler)
{
    if (storageId.empty())
        return handler(Result(ResultCode::badRequest, "Empty storageId"));

    auto storage = std::make_shared<std::optional<Storage>>();

    m_storageDao->queryExecutor().executeUpdate(
        [this, guard = m_asyncCounter.getScopedIncrement(), storageId, storage](auto queryContext)
        {
            NX_VERBOSE(this, "removeStorage: storageId: %1", storageId);

            *storage = m_storageDao->readStorage(queryContext, storageId);
            if (!storage)
                return nx::sql::DBResult::ok;

            // NOTE: storage can't be removed if there are already systems associated with it.
            if (!(**storage).systems.empty())
                return nx::sql::DBResult::ok;

            return m_storageDao->removeStorage(queryContext, storageId);
        },
        [this, guard = m_asyncCounter.getScopedIncrement(), authInfo = std::move(authInfo),
            storage, storageId, handler = std::move(handler)](
                auto dbResult) mutable
        {
            handler(prepareRemoveStorageResult(authInfo, storageId, dbResult, *storage));
        });
}

void StorageManager::listCameras(
    const std::string& /*storageId*/,
    nx::utils::MoveOnlyFunc<void(Result, std::vector<std::string>)> handler)
{
    handler(Result{ResultCode::ok, "listCamerasOk"}, std::vector<std::string>());
}

void StorageManager::getCredentials(
    nx::utils::stree::ResourceContainer authInfo,
    std::string storageId,
    nx::utils::MoveOnlyFunc<void(Result, StorageCredentials)> handler)
{
    auto readStorageContext =
        std::make_shared<StorageManager::ReadStorageContext>(
            std::move(storageId),
            std::move(authInfo),
            false /*withDataUsage*/,
            [this, handler = std::move(handler)](auto result, auto storage) mutable
            {
                if (result.resultCode != ResultCode::ok)
                    return handler(std::move(result), StorageCredentials());

                getCredentialsForStorage(std::move(storage), std::move(handler));
            });

    getStorage(std::move(readStorageContext));
}

void StorageManager::addSystem(
    nx::utils::stree::ResourceContainer authInfo,
    std::string storageId,
    AddSystemRequest request,
    nx::utils::MoveOnlyFunc<void(Result, System)> handler)
{
    modifySystemStorageRelation(
        "Add",
        std::move(authInfo),
        storageId,
        request.id,
        std::bind(&AbstractStorageDao::addSystem, m_storageDao, _1, _2),
        std::move(handler));
}

void StorageManager::removeSystem(
    nx::utils::stree::ResourceContainer authInfo,
    std::string storageId,
    std::string systemId,
    nx::utils::MoveOnlyFunc<void(Result)> handler)
{
    modifySystemStorageRelation(
        "Remove",
        std::move(authInfo),
        storageId,
        systemId,
        std::bind(&AbstractStorageDao::removeSystem, m_storageDao, _1, _2),
        [handler = std::move(handler)](auto result, auto /*system*/)
        {
            handler(result);
        });
}

void StorageManager::getStorage(
    std::shared_ptr<StorageManager::ReadStorageContext> readStorageContext)
{
    if (readStorageContext->storageId.empty())
    {
        return readStorageContext->handler(
            Result(ResultCode::badRequest,"Empty storageId"),
            Storage());
    }

    m_storageDao->queryExecutor().executeSelect(
        [this, guard = m_asyncCounter.getScopedIncrement(), readStorageContext](auto queryContext)
        {
            auto storage = m_storageDao->readStorage(queryContext, readStorageContext->storageId);
            readStorageContext->found = storage.has_value();
            if (storage)
                readStorageContext->storage = std::move(*storage);

            // TODO where to get information about storage type?
            for (auto& device : readStorageContext->storage.ioDevices)
                device.type = kStorageType;

            return nx::sql::DBResult::ok;
        },
        [this, guard = m_asyncCounter.getScopedIncrement(), readStorageContext](
            auto dbResult) mutable
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                Result error(
                    utils::toResultCode(dbResult),
                    lm("getStorage %1 failed with sql error: %2")
                        .args(readStorageContext->storageId, toString(dbResult)).toStdString());
                NX_ERROR(this, error.error);
                return readStorageContext->handler(std::move(error), Storage());
            }

            if (!readStorageContext->found)
            {
                NX_VERBOSE(this, "getStorage failed: storageId %1 not found",
                    readStorageContext->storageId);
                return readStorageContext->handler(
                    Result(ResultCode::notFound),
                    Storage());
            }

            checkReadStorageAllowed(std::move(readStorageContext));
        });
}

void StorageManager::checkReadStorageAllowed(
    std::shared_ptr<StorageManager::ReadStorageContext> readStorageContext)
{
    m_accessManager->readStorageAllowed(
        readStorageContext->authInfo,
        readStorageContext->storage,
        [this, readStorageContext](bool allowed)
        {
            if (!allowed)
            {
                NX_VERBOSE(this,
                    "checkReadStorageAllowed: unauthorized user %1 attempting to accessing storage %2",
                        m_accessManager->getAccountEmail(readStorageContext->authInfo),
                        readStorageContext->storageId);

                return readStorageContext->handler(
                    Result(ResultCode::unauthorized),
                    Storage());
            }

            if (!readStorageContext->withDataUsage)
            {
                return readStorageContext->handler(
                    Result(),
                    std::move(readStorageContext->storage));
            }

            calculateDataUsage(
                std::move(readStorageContext->storage),
                std::move(readStorageContext->handler));
        });
}

void StorageManager::getCredentialsForStorage(
    Storage storage,
    nx::utils::MoveOnlyFunc<void(Result, StorageCredentials)> handler)
{
    aws::sts::AssumeRoleRequest request;
    request.policy = buildStorageAccessPolicy(storage);
    request.roleArn = m_settings.aws().assumeRoleArn;
    request.roleSessionName =
        QnUuid::createUuid().toSimpleString().toStdString();
    request.durationSeconds = (int) m_settings.aws().storageCredentialsDuration.count();

    m_stsClient->assumeRole(
        request,
        [this, handler = std::move(handler), storage = std::move(storage)](
            auto awsResult,
            auto assumeRoleResult)
        {
            if (!awsResult.ok())
            {
                NX_INFO(this, "sts:AssumeRole failed: %1", awsResult.text());
                return handler(utils::toResult(awsResult), StorageCredentials());
            }

            StorageCredentials credentials;
            credentials.login = std::move(assumeRoleResult.credentials.accessKeyId);
            credentials.password = std::move(assumeRoleResult.credentials.secretAccessKey);
            credentials.durationSeconds =
                (int) m_settings.aws().storageCredentialsDuration.count();
            for (const auto& ioDevice : storage.ioDevices)
                credentials.urls.emplace_back(ioDevice.dataUrl);

            handler(Result(), std::move(credentials));
        });
}

std::pair<Result, Bucket> StorageManager::findBucketByRegion(
    const std::vector<Bucket>& buckets,
    const std::string& region)
{
    NX_VERBOSE(this, "findClosestBucket, region: %1", region);

    std::optional<Bucket> chosenBucket;
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
        Result error(
            ResultCode::notFound,
            lm("No bucket found in region: %1").arg(region).toStdString());
        NX_VERBOSE(this, error.error);
        return std::make_pair(std::move(error), Bucket());
    }

    return std::make_pair(Result(), std::move(*chosenBucket));
}

std::pair<Result, Bucket> StorageManager::findClosestBucket(
    const std::vector<Bucket>& buckets,
    const network::SocketAddress& clientEndpoint)
{
    NX_VERBOSE(this, "findClosestBucket, clientEndpoint: %1", clientEndpoint);

    auto clientLocation = resolveLocation(clientEndpoint.address.toStdString());
    if (!clientLocation)
    {
        Result error(
            ResultCode::internalError,
            lm("Failed to resolve client IP %1 to a location")
                .arg(clientEndpoint.address.toString()).toStdString());
        NX_VERBOSE(this, error.error);
        return std::make_pair(std::move(error), Bucket());
    }

    std::multimap<nx::geo_ip::Kilometers, Bucket> bucketsByDistance;
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
        Result error(
            ResultCode::internalError,
            "No available buckets to use for adding storage");
        NX_ERROR(this, error.error);
        return std::make_pair(std::move(error), Bucket());
    }

    return std::make_pair(Result(), bucketsByDistance.begin()->second);
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

void StorageManager::calculateDataUsage(
    Storage storage,
    nx::utils::MoveOnlyFunc<void(Result, Storage)> handler)
{
    auto calculator = createDataUsageCalculator();

    std::vector<s3::DataUsageCalculator::Bucket> buckets;
    std::transform(
        storage.ioDevices.begin(),
        storage.ioDevices.end(),
        std::back_inserter(buckets),
        [](const auto& device)
        {
            return s3::DataUsageCalculator::Bucket{device.region, device.dataUrl};
        });

    NX_VERBOSE(this, "Calculating data usage for storageId %1, buckets: %2",
        storage.id, containerString(buckets));

    calculator->calculate(
        std::move(buckets),
        [this, storage = std::move(storage), handler = std::move(handler), calculator](
            auto result,
            int bytesUsed) mutable
        {
            removeDataUsageCalculator(calculator);

            if (result.resultCode != ResultCode::ok)
            {
                NX_VERBOSE(this, "calculateDataUsage for storageId %1 failed: %2, %3",
                    storage.id, toString(result.resultCode), result.error);
                return handler(std::move(result), Storage());
            }

            storage.freeSpace = std::max(0, storage.totalSpace - bytesUsed);

            handler(Result(), std::move(storage));
        });
}

std::shared_ptr<s3::DataUsageCalculator> StorageManager::createDataUsageCalculator()
{
    QnMutexLocker lock(&m_mutex);
    auto dataUsageCalculator =
        std::make_shared<s3::DataUsageCalculator>(m_settings.aws().credentials);
    m_dataUsageCalculators.emplace(dataUsageCalculator);
    return dataUsageCalculator;
}

void StorageManager::removeDataUsageCalculator(
    const std::shared_ptr<s3::DataUsageCalculator>& calculator)
{
    QnMutexLocker lock(&m_mutex);
    m_dataUsageCalculators.erase(calculator);
}

Result StorageManager::prepareRemoveStorageResult(
    nx::utils::stree::ResourceContainer& authInfo,
    const std::string& storageId,
    nx::sql::DBResult dbResult,
    const std::optional<Storage>& storage) const
{
    if (dbResult != nx::sql::DBResult::ok)
    {
        Result error(
            utils::toResultCode(dbResult),
            lm("removeStorage %1 failed with sql error: %2")
                .args(storageId, toString(dbResult)).toStdString());
        NX_ERROR(this, error.error);
        return error;
    }

    if (!storage)
    {
        NX_VERBOSE(this, "removeStorage %1 failed, storage not found", storageId);
        return ResultCode::notFound;
    }

    if (!m_accessManager->isStorageOwner(authInfo, *storage))
    {
        NX_VERBOSE(this, "removeStorage %1 attempted by unauthorized user %2, rejecting request",
            storageId, m_accessManager->getAccountEmail(authInfo));
        return ResultCode::unauthorized;
    }

    if (!storage->systems.empty())
    {
        Result error(
            ResultCode::unauthorized,
            lm("Storage %1 has at least one system associated with it").arg(storageId).toStdString());
        NX_VERBOSE(this, "removeStorage %1 failed: %2", storageId, error.error);
        return error;
    }

    return Result();
}

template<typename DbFunc, typename Handler>
void StorageManager::modifySystemStorageRelation(
    const char* operation,
    nx::utils::stree::ResourceContainer authInfo,
    std::string storageId,
    std::string systemId,
    DbFunc dbFunc,
    Handler handler)
{
    if (storageId.empty())
        return handler(Result(ResultCode::badRequest, "Empty storageId"), System());
    if (systemId.empty())
        return handler(Result(ResultCode::badRequest, "Empty systemId"), System());

    auto storage = std::make_shared<std::optional<Storage>>();
    auto system = std::make_shared<System>();
    system->storageId = storageId;
    system->id = systemId;

    m_storageDao->queryExecutor().executeUpdate(
        [this, guard = m_asyncCounter.getScopedIncrement(), storage, system,
            operation, dbFunc = std::move(dbFunc)](
                auto queryContext)
        {
            NX_VERBOSE(this, "%1 System request: storageId: %2, systemId: %3",
                operation, system->storageId, system->id);
            *storage = m_storageDao->readStorage(queryContext, system->storageId);
            if (!*storage)
                return nx::sql::DBResult::ok;

            return dbFunc(queryContext, *system);
        },
        [this, guard = m_asyncCounter.getScopedIncrement(), authInfo = std::move(authInfo),
            storage, system, operation, handler = std::move(handler)](
                auto dbResult)
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                Result error(
                    utils::toResultCode(dbResult),
                    lm("%1 System request with storageId: %2, systemId: %3 failed with sql error: %4")
                    .args(operation, system->storageId, system->id, toString(dbResult)).toStdString());
                NX_ERROR(this, error.error);
                return handler(std::move(error), System());
            }

            if (!*storage)
            {
                NX_VERBOSE(this, "%1 System request: storageId %2 not found",
                    operation, system->storageId);
                return handler(ResultCode::notFound, System());
            }

            if (!m_accessManager->isStorageOwner(authInfo, **storage))
            {
                NX_VERBOSE(this, "%1 System request with storageId: %2, systemId: %3 rejected:"
                    "unauthorized user %4 attempted to access storage",
                         operation, system->storageId, system->id,
                         m_accessManager->getAccountEmail(authInfo));
                return handler(ResultCode::unauthorized, System());
            }

            handler(Result(), std::move(*system));
        });
}

} // namespace nx::cloud::storage::service::controller
