#include "storage_manager.h"

#include <nx/network/socket_common.h>
#include <nx/network/url/url_builder.h>
#include <nx/cloud/aws/sts/api_client.h>
#include <nx/utils/stree/resourcecontainer.h>

#include "nx/cloud/storage/service/settings.h"
#include "nx/cloud/storage/service/utils.h"
#include "nx/cloud/storage/service/model/model.h"
#include "nx/cloud/storage/service/model/database.h"
#include "nx/cloud/storage/service/model/dao/storage_dao.h"
#include "access_manager.h"
#include "utils.h"

namespace nx::cloud::storage::service::controller {

using namespace std::placeholders;
using namespace model::dao;

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

static std::string buildStorageAccessPolicy(const api::Storage& storage)
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
    const api::Bucket& bucket,
    const api::AddStorageRequest& request)
{
    storage.id = QnUuid::createUuid().toSimpleString().toStdString();
    storage.totalSpace = request.totalSpace;
    storage.freeSpace = request.totalSpace;
    storage.ioDevices.emplace_back(api::Device());
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
    BaseManager(
        settings.database().synchronization.clusterId,
        &model->database().synchronizationEngine()),
    m_settings(settings),
    m_database(&model->database()),
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
    registerSyncEngineCommandHandlers();
}

StorageManager::~StorageManager()
{
    m_stsClient->pleaseStopSync();
    QnMutexLocker lock(&m_mutex);
    for (auto& calculator: m_dataUsageCalculators)
        calculator->pleaseStopSync();
}

void StorageManager::addStorage(
    nx::utils::stree::ResourceContainer authInfo,
    network::SocketAddress clientEndpoint,
    api::AddStorageRequest request,
    GetStorageHandler handler)
{
    if (request.totalSpace <= 0)
    {
        return handler(
            utils::badRequest(lm("Invalid storage size: %1").arg(request.totalSpace)),
            api::Storage());
    }

    auto [allowed, accountOwner] = m_accessManager->addStorageAllowed(authInfo);
    if (!allowed)
        return handler(api::ResultCode::unauthorized, api::Storage());

    auto addStorageContext = std::make_shared<AddStorageContext>();

    m_database->synchronizationEngine().transactionLog().startDbTransaction(
        m_settings.database().synchronization.clusterId,
        [this, addStorageContext, request, clientEndpoint, accountOwner = accountOwner](auto queryContext)
        {
            auto buckets = m_bucketManager->fetchBuckets(queryContext);

            auto [result, bucket] = request.region.empty()
                ? findClosestBucket(buckets, clientEndpoint)
                : findBucketByRegion(buckets, request.region);

            addStorageContext->bucketLookupResult = std::move(result);
            if (addStorageContext->bucketLookupResult.resultCode != api::ResultCode::ok)
                return nx::sql::DBResult::ok;

            addStorageContext->initializeStorage(accountOwner, bucket, request);

            return modifyDbAndSynchronize<command::SaveStorage>(
                queryContext,
                addStorageContext->storage,
                std::bind(&AbstractStorageDao::addStorage, m_storageDao, _1, _2));
        },
        [this, handler = std::move(handler), addStorageContext](auto dbResult)
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                api::Result error(utils::toResultCode(dbResult));
                error.error = lm("addStorage failed with sql error: %1")
                    .arg(toString(dbResult)).toStdString();
                NX_ERROR(this, error.error);
                return handler(std::move(error), api::Storage());
            }

            if (addStorageContext->bucketLookupResult.resultCode != api::ResultCode::ok)
                return handler(std::move(addStorageContext->bucketLookupResult), api::Storage());

            handler(api::Result(), std::move(addStorageContext->storage));
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
    nx::utils::MoveOnlyFunc<void(api::Result)> handler)
{
    if (storageId.empty())
        return handler(utils::badRequest("Empty storageId"));

    auto storage = std::make_shared<std::optional<api::Storage>>();

    m_database->synchronizationEngine().transactionLog().startDbTransaction(
        m_settings.database().synchronization.clusterId,
        [this, storageId, storage](auto queryContext)
        {
            *storage = m_storageDao->readStorage(queryContext, storageId);
            if (!storage)
                return nx::sql::DBResult::ok;

            return modifyDbAndSynchronize<command::RemoveStorage>(
                queryContext,
                storageId,
                std::bind(&AbstractStorageDao::removeStorage, m_storageDao, _1, _2));
        },
        [this, authInfo = std::move(authInfo), storageId, storage,
            handler = std::move(handler)](
                auto dbResult)
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
                return handler(api::ResultCode::ok);

            if (!m_accessManager->isStorageOwner(authInfo, **storage))
                return handler(api::ResultCode::unauthorized);

            handler(api::Result());
        });
}

void StorageManager::listCameras(
    const std::string& /*storageId*/,
    nx::utils::MoveOnlyFunc<void(api::Result, std::vector<std::string>)> handler)
{
    handler(api::Result{api::ResultCode::ok, "listCamerasOk"}, std::vector<std::string>());
}

void StorageManager::getCredentials(
    nx::utils::stree::ResourceContainer authInfo,
    std::string storageId,
    nx::utils::MoveOnlyFunc<void(api::Result, api::StorageCredentials)> handler)
{
    // TODO decide if withDataUsage is necessary, e.g. data usage is always calculated.
    // it can be used to deny credentials if storage utilization exceeds storage size
    auto readStorageContext =
        std::make_shared<StorageManager::ReadStorageContext>(
            std::move(storageId),
            std::move(authInfo),
            false /*withDataUsage*/,
            [this, handler = std::move(handler)](auto result, auto storage) mutable
            {
                if (result.resultCode != api::ResultCode::ok)
                    return handler(std::move(result), api::StorageCredentials());

                getCredentialsForStorage(std::move(storage), std::move(handler));
            });

    getStorage(std::move(readStorageContext));
}

void StorageManager::addSystem(
    nx::utils::stree::ResourceContainer authInfo,
    std::string storageId,
    api::AddSystemRequest request,
    nx::utils::MoveOnlyFunc<void(api::Result, api::System)> handler)
{
    modifySystemStorageRelation<command::SaveSystem>(
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
    nx::utils::MoveOnlyFunc<void(api::Result)> handler)
{
    modifySystemStorageRelation<command::RemoveSystem>(
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
        return readStorageContext->handler(utils::badRequest("Empty storageId"), api::Storage());

    m_database->synchronizationEngine().transactionLog().startDbTransaction(
        m_settings.database().synchronization.clusterId,
        [this, readStorageContext](auto queryContext)
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
        [this, readStorageContext](auto dbResult) mutable
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                api::Result error(utils::toResultCode(dbResult));
                error.error = lm("readStorage(%1) failed with sql error: %2")
                    .args(readStorageContext->storageId, toString(dbResult)).toStdString();
                NX_ERROR(this, error.error);
                return readStorageContext->handler(std::move(error), api::Storage());
            }

            if (!readStorageContext->found)
            {
                return readStorageContext->handler(
                    api::Result(api::ResultCode::notFound),
                    api::Storage());
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
                return readStorageContext->handler(
                    api::Result(api::ResultCode::unauthorized),
                    api::Storage());
            }

            if (!readStorageContext->withDataUsage)
            {
                return readStorageContext->handler(
                    api::Result(),
                    std::move(readStorageContext->storage));
            }

            calculateDataUsage(
                std::move(readStorageContext->storage),
                std::move(readStorageContext->handler));
        });
}

void StorageManager::getCredentialsForStorage(
    api::Storage storage,
    nx::utils::MoveOnlyFunc<void(api::Result, api::StorageCredentials)> handler)
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
                NX_ERROR(this, "sts:AssumeRole failed: %1", awsResult.text());
                return handler(utils::toResult(awsResult), api::StorageCredentials());
            }

            api::StorageCredentials credentials;
            credentials.login = std::move(assumeRoleResult.credentials.accessKeyId);
            credentials.password = std::move(assumeRoleResult.credentials.secretAccessKey);
            credentials.durationSeconds =
                (int) m_settings.aws().storageCredentialsDuration.count();
            for (const auto& ioDevice : storage.ioDevices)
                credentials.urls.emplace_back(ioDevice.dataUrl);

            handler(api::Result(), std::move(credentials));
        });
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
        return std::make_pair(std::move(error), api::Bucket());
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
        return std::make_pair(std::move(error), api::Bucket());
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
        return std::make_pair(std::move(error), api::Bucket());
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

void StorageManager::calculateDataUsage(
    api::Storage storage,
    nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)> handler)
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

    calculator->calculate(
        std::move(buckets),
        [this, storage = std::move(storage), handler = std::move(handler), calculator](
            auto result,
            int bytesUsed) mutable
        {
            removeDataUsageCalculator(calculator);

            if (result.resultCode != api::ResultCode::ok)
                return handler(std::move(result), api::Storage());

            storage.freeSpace = std::max(0, storage.totalSpace - bytesUsed);

            handler(api::Result(), std::move(storage));
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

void StorageManager::registerSyncEngineCommandHandlers()
{
    registerCommandHandler<command::SaveStorage>(
        std::bind(&AbstractStorageDao::addStorage, m_storageDao, _1, _2));

    registerCommandHandler<command::RemoveStorage>(
        std::bind(&AbstractStorageDao::removeStorage, m_storageDao, _1, _2));

    registerCommandHandler<command::SaveSystem>(
        std::bind(&AbstractStorageDao::addSystem, m_storageDao, _1, _2));

    registerCommandHandler<command::RemoveSystem>(
        std::bind(&AbstractStorageDao::removeSystem, m_storageDao, _1, _2));
}

template<typename Command, typename DbFunc, typename Handler>
void StorageManager::modifySystemStorageRelation(
    nx::utils::stree::ResourceContainer authInfo,
    std::string storageId,
    std::string systemId,
    DbFunc dbFunc,
    Handler handler)
{
    if (storageId.empty())
        return handler(utils::badRequest("Empty storageId"), api::System());
    if (systemId.empty())
        return handler(utils::badRequest("Empty systemId"), api::System());

    auto storage = std::make_shared<std::optional<api::Storage>>();
    auto system = std::make_shared<api::System>();
    system->storageId = storageId;
    system->id = systemId;

    m_database->synchronizationEngine().transactionLog().startDbTransaction(
        m_settings.database().synchronization.clusterId,
        [this, storage, system, dbFunc = std::move(dbFunc)](auto queryContext)
        {
            *storage = m_storageDao->readStorage(queryContext, system->storageId);
            if (!*storage)
                return nx::sql::DBResult::ok;

            return modifyDbAndSynchronize<Command>(
                queryContext,
                *system,
                std::move(dbFunc));
        },
        [this, authInfo = std::move(authInfo), storage, system,
            handler = std::move(handler)](
                auto dbResult)
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                api::Result error(
                    utils::toResultCode(dbResult),
                    lm("Request with storageId:%1, systemId:%2 failed with sql error: %3")
                    .args(system->storageId, system->id, toString(dbResult)).toStdString());
                NX_ERROR(this, error.error);
                return handler(std::move(error), api::System());
            }

            if (!*storage)
                return handler(api::ResultCode::notFound, api::System());

            if (!m_accessManager->isStorageOwner(authInfo, **storage))
                return handler(api::ResultCode::unauthorized, api::System());

            handler(api::Result(), std::move(*system));
        });
}

} // namespace nx::cloud::storage::service::controller
