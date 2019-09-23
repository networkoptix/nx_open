#include "authorization_manager.h"

#include <nx/cloud/db/api/auth_provider.h>
#include <nx/cloud/db/api/cdb_client.h>
#include <nx/cloud/storage/service/api/storage.h>
#include <nx/utils/stree/resourcecontainer.h>

#include "nx/cloud/storage/service/settings.h"
#include "cloud_db/resource.h"

namespace nx::cloud::storage::service::controller {

using namespace nx::cloud::storage::service::api;

namespace {

static std::vector<db::api::SystemAccessLevelRequest> prepareBatchRequest(
    const std::vector<std::string> systemIds,
    const db::api::UserAuthorization& userAuth)
{
    std::vector<nx::cloud::db::api::SystemAccessLevelRequest> batchRequest;
    std::transform(
        systemIds.begin(),
        systemIds.end(),
        std::back_inserter(batchRequest),
        [&userAuth](const auto& systemId)
        {
            return db::api::SystemAccessLevelRequest{systemId, userAuth};
        });
    return batchRequest;
}

static QString toString(const db::api::UserAuthorization& userAuth)
{
    return lm("{requestMethod: %1, requestAuthorization: %2}")
        .args(userAuth.requestMethod, userAuth.requestAuthorization);
}

static ResultCode toResultCode(db::api::ResultCode resultCode)
{
    switch (resultCode)
    {
        case db::api::ResultCode::ok:
            return api::ResultCode::ok;
        case db::api::ResultCode::notAuthorized:
        case db::api::ResultCode::notFound:
        case db::api::ResultCode::accountBlocked:
		case db::api::ResultCode::forbidden:
			// Any failure in cloud_db at this point is forbidden as original request to
			// Cloud Storage Service has already been authenticated.
			return api::ResultCode::forbidden;
        default:
            return api::ResultCode::internalError;
    }
}

} // namespace

AuthorizationManager::AuthorizationManager(const conf::CloudDb& settings):
    m_settings(settings)
{
}

AuthorizationManager::~AuthorizationManager()
{
    stop();
};

void AuthorizationManager::stop()
{
    QnMutexLocker lock(&m_mutex);
    auto cdbRequests = std::move(m_cdbRequests);
    lock.unlock();

    cdbRequests.clear();
}

std::pair<Result, std::string> AuthorizationManager::authorizeAddingStorage(
    const nx::utils::stree::ResourceContainer& authInfo) const
{
	// getAccountEmail returns an empty string, it is because a non account entity is making
	// the request, e.g. a system. Only Cloud account entities are allowed to add storage.
    auto accountEmail = this->getAccountEmail(authInfo);
    auto result = accountEmail.empty() ? ResultCode::forbidden : ResultCode::ok;
    return std::make_pair(std::move(result), std::move(accountEmail));
}

void AuthorizationManager::authorizeReadingStorage(
    const nx::utils::stree::ResourceContainer& authInfo,
    const Storage& storage,
    AuthorizeReadingStorageHandler handler)
{
    auto accountEmail = getAccountEmail(authInfo);
    if (accountEmail == storage.owner)
        return handler(ResultCode::ok);

    // Non-owner trying to access storage, check by user's access to a system in the storage

    if (storage.systems.empty())
        return handler(ResultCode::forbidden);

    db::api::UserAuthorization userAuth;
    authInfo.get(cloud_db::Resource::httpMethod, &userAuth.requestMethod);
    authInfo.get(cloud_db::Resource::authorization, &userAuth.requestAuthorization);

    auto& readStorageContext = createReadStorageContext();
    readStorageContext.handler = std::move(handler);

    readStorageContext.cdbClient->authProvider()->getSystemAccessLevel(
        prepareBatchRequest(storage.systems, userAuth),
        [this, userAuth, cdbClient = readStorageContext.cdbClient.get(), accountEmail](
            auto cdbResult,
            auto systemAccessLevels)
        {
            auto context = takeReadStorageContext(cdbClient);
            if (cdbResult != db::api::ResultCode::ok)
            {
                Result error(
                    toResultCode(cdbResult),
                    lm("cloud_db: getSystemAccessLevel for user %1 failed with result code: %2")
                        .args(accountEmail, db::api::toString(cdbResult)).toStdString());
                NX_VERBOSE(this, "%1, userAuth was %2", error, toString(userAuth));
                return context.handler(std::move(error));
            }

            auto it = std::find_if(systemAccessLevels.begin(), systemAccessLevels.end(),
                [](const auto& systemAccess)
                {
                    return systemAccess.accessRole > db::api::SystemAccessRole::disabled;
                });

            context.handler(it == systemAccessLevels.end()
                ? ResultCode::forbidden
                : ResultCode::ok);
        });
}

std::string AuthorizationManager::getAccountEmail(
	const nx::utils::stree::ResourceContainer& authInfo) const
{
    std::string accountOwner;
    authInfo.get(cloud_db::Resource::accountEmail, &accountOwner);
    return accountOwner;
}

bool AuthorizationManager::isStorageOwner(
    const nx::utils::stree::ResourceContainer& authInfo,
    const Storage& storage) const
{
    return getAccountEmail(authInfo) == storage.owner;
}

AuthorizationManager::ReadStorageContext& AuthorizationManager::createReadStorageContext()
{
    auto cdbClient = std::make_unique<db::api::CdbClient>();
    cdbClient->setCloudUrl(m_settings.url.toStdString());
    auto cdbClientPtr = cdbClient.get();

    ReadStorageContext readStorageContext;
    readStorageContext.cdbClient = std::move(cdbClient);

    QnMutexLocker lock(&m_mutex);
    return m_cdbRequests.emplace(cdbClientPtr, std::move(readStorageContext)).first->second;
}

AuthorizationManager::ReadStorageContext AuthorizationManager::takeReadStorageContext(
    db::api::CdbClient* cdbClient)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_cdbRequests.find(cdbClient);
    NX_ASSERT(it != m_cdbRequests.end());
    auto readStorageContext = std::move(it->second);
    m_cdbRequests.erase(it);

    return readStorageContext;
}

} // namespace nx::cloud::storage::service::controller