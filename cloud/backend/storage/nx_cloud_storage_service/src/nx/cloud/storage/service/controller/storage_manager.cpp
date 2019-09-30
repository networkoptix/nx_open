#include "storage_manager.h"

#include <nx/network/socket_common.h>
#include <nx/network/url/url_builder.h>
#include <nx/cloud/aws/sts/api_client.h>
#include <nx/utils/stree/resourcecontainer.h>
#include <nx/utils/string.h>

#include "nx/cloud/storage/service/settings.h"
#include "nx/cloud/storage/service/utils.h"
#include "nx/cloud/storage/service/model/command.h"
#include "nx/cloud/storage/service/model/model.h"
#include "nx/cloud/storage/service/model/database.h"
#include "nx/cloud/storage/service/model/dao/storage_dao.h"
#include "authorization_manager.h"
#include "utils.h"

namespace nx::cloud::storage::service::controller {

using namespace std::placeholders;
using namespace model::dao;
using namespace nx::cloud::storage::service::api;

namespace {

static constexpr char kStorageType[] = "awss3";
static constexpr char kAddSystem[] = "Add System";
static constexpr char kRemoveSystem[] = "Remove System";

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

// %1 is an array of arns with the form: "arn:aws:s3:::examplebucket/folder/*".
// NOTE: Because storages can be merged, access must be granted to multiple buckets/folders.
// There is no way to know which specific bucket/folder will be be written to, access must be
// granted to all of them.
// NOTE: the entire policy is on one line because aws complains with "Signature does not match"
// error code otherwise.
static constexpr char kStorageAccessPolicyTemplate[] =
R"json({"Version":"2012-10-17","Statement":[{"Effect":"Allow","Action":["s3:GetObject","s3:PutObject","s3:DeleteObject"],"Resource":[%1]}]})json";

static std::string buildStorageAccessPolicy(const Storage& storage)
{
    const auto bucketName =
        [](const auto& item) { return service::utils::bucketName(item.dataUrl); };
    const auto storageFolder =
        [](const auto& item) { return service::utils::storageFolder(item.dataUrl); };

    auto arn = nx::utils::join(
        storage.ioDevices.begin(), storage.ioDevices.end(),
        ',',
        "arn:aws:s3:::", bucketName, "/", storageFolder, "/*");

    return lm(kStorageAccessPolicyTemplate).arg(arn).toStdString();
}

} // namespace

void StorageManager::AddStorageContext::initializeStorage(const Bucket& bucket)
{
    storage.id = QnUuid::createUuid().toSimpleString().toStdString();
    storage.totalSpace = request.totalSpace;
    storage.freeSpace = request.totalSpace;
    storage.ioDevices.emplace_back(Device());
    storage.ioDevices.back().type = kStorageType;
    storage.ioDevices.back().dataUrl =
        network::url::Builder(service::utils::bucketUrl(bucket.name)).setPath(storage.id);
    storage.ioDevices.back().region = bucket.region;
    storage.owner = std::move(accountEmail);
}

StorageManager::ReadStorageContext::ReadStorageContext(
    std::string storageId,
    nx::utils::stree::ResourceContainer authInfo,
    bool withDataUsage,
    GetStorageHandler handler)
    :
    withDataUsage(withDataUsage),
    handler(std::move(handler))
{
    this->storageId = std::move(storageId);
    this->authInfo = std::move(authInfo);
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
    m_authorizationManager(std::make_unique<AuthorizationManager>(settings.cloudDb())),
    m_geoIpResolver(GeoIpResolverFactory::instance().create(settings)),
    m_stsClient(
        std::make_unique<aws::sts::ApiClient>(
            utils::kDefaultBucketRegion,
            m_settings.aws().stsUrl,
            m_settings.aws().credentials))
{
}

StorageManager::~StorageManager()
{
    stop();
}

void StorageManager::stop()
{
    m_authorizationManager->stop();
    m_stsClient->pleaseStopSync();
    m_asyncCounter.wait();

    QnMutexLocker lock(&m_mutex);
    auto dataUsageCalculators = std::move(m_dataUsageCalculators);
    lock.unlock();

    for (auto& calculator : dataUsageCalculators)
        calculator->pleaseStopSync();
}

void StorageManager::addStorage(
    nx::utils::stree::ResourceContainer authInfo,
    network::SocketAddress clientEndpoint,
    AddStorageRequest request,
    GetStorageHandler handler)
{
    NX_VERBOSE(this, "addStorage, region: %1, clientEndpoint: %2", request.region, clientEndpoint);

    auto [validationResult, accountEmail] = validateAddStorageRequest(authInfo, request);
    if (!validationResult.ok())
        return handler(std::move(validationResult), Storage());

    auto addStorageContext = std::make_shared<AddStorageContext>();
    addStorageContext->accountEmail = std::move(accountEmail);
    addStorageContext->request = std::move(request);
    addStorageContext->clientEndpoint = std::move(clientEndpoint);

    m_storageDao->queryExecutor().executeUpdate(
        [this, guard = m_asyncCounter.getScopedIncrement(), addStorageContext](auto queryContext)
        {
            addStorageToDb(addStorageContext.get(), queryContext);
        },
        [this, guard = m_asyncCounter.getScopedIncrement(), handler = std::move(handler),
            addStorageContext](
            auto dbResult)
        {
            auto [result, storage] = prepareAddStorageResult(addStorageContext.get(), dbResult);
            handler(std::move(result), std::move(storage));
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
    std::string storageId,
    nx::utils::MoveOnlyFunc<void(Result)> handler)
{
    if (storageId.empty())
        return handler(Result(ResultCode::badRequest, "Empty storageId"));

    auto removeStorageContext = std::make_shared<CommonStorageContext>();
    removeStorageContext->authInfo = std::move(authInfo);
    removeStorageContext->storageId = std::move(storageId);

    m_storageDao->queryExecutor().executeUpdate(
        [this, guard = m_asyncCounter.getScopedIncrement(), removeStorageContext](
            auto queryContext)
        {
            return removeStorageFromDb(removeStorageContext.get(), queryContext);
        },
        [this, guard = m_asyncCounter.getScopedIncrement(), removeStorageContext,
            handler = std::move(handler)](
                auto dbResult) mutable
        {
            handler(prepareRemoveStorageResult(removeStorageContext.get(), dbResult));
        });
}

void StorageManager::listCameras(
    std::string /*storageId*/,
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
		kAddSystem,
        std::move(authInfo),
        std::move(storageId),
        std::move(request.id),
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
		kRemoveSystem,
        std::move(authInfo),
        std::move(storageId),
		std::move(systemId),
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
			if (storage)
				readStorageContext->storage = std::move(*storage);
			else
				readStorageContext->result = Result(ResultCode::notFound, "No such storage");

            // TODO where to get information about storage type?
            for (auto& device : readStorageContext->storage.ioDevices)
                device.type = kStorageType;
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

            if (!readStorageContext->result.ok())
            {
                NX_VERBOSE(this, "getStorage %1 failed: %2",
                    readStorageContext->storageId, readStorageContext->result);
                return readStorageContext->handler(std::move(readStorageContext->result), Storage());
            }

            authorizeReadingStorage(std::move(readStorageContext));
        });
}

void StorageManager::authorizeReadingStorage(
    std::shared_ptr<StorageManager::ReadStorageContext> readStorageContext)
{
    m_authorizationManager->authorizeReadingStorage(
        readStorageContext->authInfo,
        readStorageContext->storage,
        [this, readStorageContext](auto result)
        {
            if (!result.ok())
            {
                NX_VERBOSE(this,
                    "readStorage %1 for user %2 failed: %3",
                        readStorageContext->storageId,
                        m_authorizationManager->getAccountEmail(readStorageContext->authInfo),
                        result);
                return readStorageContext->handler(std::move(result), Storage());
            }

            if (!readStorageContext->withDataUsage)
            {
                return readStorageContext->handler(
                    ResultCode::ok,
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

    NX_VERBOSE(this, "getCredentials for storage %1", storage.id);

    m_stsClient->assumeRole(
        request,
        [this, handler = std::move(handler), storage = std::move(storage)](
            auto awsResult,
            auto assumeRoleResult)
        {
            NX_VERBOSE(this, "sts:AssumeRole for storage %1 result: %2, %3",
                storage.id, aws::toString(awsResult.code()), awsResult.text());

            if (!awsResult.ok())
                return handler(utils::toResult(awsResult), StorageCredentials());

            StorageCredentials credentials;
            credentials.login = std::move(assumeRoleResult.credentials.accessKeyId);
            credentials.password = std::move(assumeRoleResult.credentials.secretAccessKey);
            credentials.sessionToken = std::move(assumeRoleResult.credentials.sessionToken);
            credentials.durationSeconds =
                (int) m_settings.aws().storageCredentialsDuration.count();
            for (const auto& ioDevice : storage.ioDevices)
                credentials.urls.emplace_back(ioDevice.dataUrl);

            handler(ResultCode::ok, std::move(credentials));
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

    return std::make_pair(ResultCode::ok, std::move(*chosenBucket));
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
        NX_WARNING(this, error.error);
        return std::make_pair(std::move(error), Bucket());
    }

    return std::make_pair(ResultCode::ok, bucketsByDistance.begin()->second);
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

void StorageManager::calculateDataUsage(Storage storage, GetStorageHandler handler)
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

            if (!result.ok())
            {
                NX_VERBOSE(this, "calculateDataUsage for storageId %1 failed: %2",
                    storage.id, result);
                return handler(std::move(result), Storage());
            }

            storage.freeSpace = std::max(0, storage.totalSpace - bytesUsed);

            handler(ResultCode::ok, std::move(storage));
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

std::pair<api::Result, std::string> StorageManager::validateAddStorageRequest(
    const nx::utils::stree::ResourceContainer& authInfo,
    const AddStorageRequest& request) const
{
    if (request.totalSpace <= 0)
    {
        return std::make_pair(
            Result(
                ResultCode::badRequest,
                lm("Invalid storage size: %1").arg(request.totalSpace).toStdString()),
            std::string());
    }

    auto [result, accountEmail] = m_authorizationManager->authorizeAddingStorage(authInfo);
    if (!result.ok())
    {
        NX_VERBOSE(this, "addStorage failed for user %1: %2", accountEmail, result);
        return std::make_pair(std::move(result), std::string());
    }

    return std::make_pair(ResultCode::ok, std::move(accountEmail));
}

void StorageManager::addStorageToDb(
    AddStorageContext* addStorageContext,
    nx::sql::QueryContext* queryContext)
{
    auto buckets = m_bucketManager->fetchBuckets(queryContext);

    auto [result, bucket] = addStorageContext->request.region.empty()
        ? findClosestBucket(buckets, addStorageContext->clientEndpoint)
        : findBucketByRegion(buckets, addStorageContext->request.region);

    addStorageContext->bucketLookupResult = std::move(result);
    if (!addStorageContext->bucketLookupResult.ok())
        return;

    addStorageContext->initializeStorage(bucket);

    m_storageDao->addStorage(queryContext, addStorageContext->storage);
}

std::pair<api::Result, api::Storage> StorageManager::prepareAddStorageResult(
    AddStorageContext* addStorageContext,
    nx::sql::DBResult dbResult) const
{
    if (dbResult != nx::sql::DBResult::ok)
    {
        Result error(
            utils::toResultCode(dbResult),
            lm("addStorage failed with sql error: %1").arg(toString(dbResult)).toStdString());
        NX_ERROR(this, error.error);
        return std::make_pair(std::move(error), Storage());
    }

    if (addStorageContext->bucketLookupResult.resultCode != ResultCode::ok)
    {
        NX_VERBOSE(this, "addBucket failed during bucket lookup: %1",
            addStorageContext->bucketLookupResult);
        return std::make_pair(std::move(addStorageContext->bucketLookupResult), Storage());
    }

    return std::make_pair(ResultCode::ok, std::move(addStorageContext->storage));
}

void StorageManager::removeStorageFromDb(
    CommonStorageContext* removeStorageContext,
    nx::sql::QueryContext* queryContext)
{
    NX_VERBOSE(this, "removeStorage: storageId: %1", removeStorageContext->storageId);

    auto storage = m_storageDao->readStorage(queryContext, removeStorageContext->storageId);
    if (!storage)
    {
		removeStorageContext->result = Result(ResultCode::notFound, "No such storage");
        return;
    }

	if (!m_authorizationManager->isStorageOwner(removeStorageContext->authInfo, *storage))
	{
		removeStorageContext->result = ResultCode::forbidden;
		return;
	}

    // NOTE: storage can't be removed if there are already systems associated with it.
    if (!storage->systems.empty())
    {
        removeStorageContext->result = Result(
            ResultCode::forbidden,
            "Storage has at least one system associated with it");
        return;
    }

    m_storageDao->removeStorage(queryContext, removeStorageContext->storageId);
}

Result StorageManager::prepareRemoveStorageResult(
    CommonStorageContext* removeStorageContext,
    nx::sql::DBResult dbResult) const
{
    if (dbResult != nx::sql::DBResult::ok)
    {
        Result error(
            utils::toResultCode(dbResult),
            lm("removeStorage %1 failed with sql error: %2")
                .args(removeStorageContext->storageId, toString(dbResult)).toStdString());
        NX_ERROR(this, error.error);
        return error;
    }

    if (!removeStorageContext->result.ok())
    {
        NX_VERBOSE(this, "removeStorage %1 failed: %2",
            removeStorageContext->storageId, removeStorageContext->result);
        return std::move(removeStorageContext->result);
    }

    return ResultCode::ok;
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

	auto context = std::make_shared<SystemStorageContext>();
	context->authInfo = std::move(authInfo);
	context->system.id = std::move(systemId);
	context->system.storageId = std::move(storageId);

    m_storageDao->queryExecutor().executeUpdate(
        [this, guard = m_asyncCounter.getScopedIncrement(), operation,
		    dbFunc = std::move(dbFunc), context](
                auto queryContext)
        {
            NX_VERBOSE(this, "%1 request: storageId: %2, systemId: %3",
                operation, context->system.storageId, context->system.id);
            auto storage = m_storageDao->readStorage(queryContext, context->system.storageId);
            if (!storage)
			{
				context->result = Result(ResultCode::notFound, "No such storage");
				return;
			}

			if (!m_authorizationManager->isStorageOwner(context->authInfo, *storage))
			{
				NX_VERBOSE(this, "%1 request with storageId: %2, systemId: %3 rejected:"
					"unauthorized user %4 attempted to access storage",
					operation, context->system.storageId, context->system.id,
					m_authorizationManager->getAccountEmail(context->authInfo));
				context->result = ResultCode::forbidden;
				return;
			}

            dbFunc(queryContext, context->system);
        },
        [this, guard = m_asyncCounter.getScopedIncrement(), operation,
			handler = std::move(handler), context](
                auto dbResult)
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                Result error(
                    utils::toResultCode(dbResult),
                    lm("%1 request with storageId: %2, systemId: %3 failed with sql error: %4")
                        .args(operation, context->system.storageId, context->system.id,
					        toString(dbResult)).toStdString());
                NX_ERROR(this, error.error);
                return handler(std::move(error), System());
            }

            if (!context->result.ok())
            {
                NX_VERBOSE(this, "%1 request failed: %2", operation, context->result);
                return handler(std::move(context->result), System());
            }

            handler(ResultCode::ok, std::move(context->system));
        });
}

} // namespace nx::cloud::storage::service::controller
