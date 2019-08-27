#include "access_manager.h"

#include <nx/cloud/db/api/auth_provider.h>
#include <nx/cloud/db/api/cdb_client.h>
#include <nx/cloud/storage/service/api/storage.h>
#include <nx/utils/stree/resourcecontainer.h>

#include "nx/cloud/storage/service/settings.h"
#include "cloud_db/resource.h"

namespace nx::cloud::storage::service::controller {

namespace {

QString toString(const db::api::UserAuthorization& userAuth)
{
    return lm("{requestMethod: %1, requestAuthorization: %2}")
        .args(userAuth.requestMethod, userAuth.requestAuthorization);
}

} // namespace

AccessManager::AccessManager(const conf::CloudDb& settings):
    m_settings(settings)
{
}

AccessManager::~AccessManager() = default;

std::pair<bool, std::string> AccessManager::addStorageAllowed(
    const nx::utils::stree::ResourceContainer& authInfo) const
{
    auto accountEmail = this->getAccountEmail(authInfo);
    return {!accountEmail.empty(), accountEmail};
}

void AccessManager::readStorageAllowed(
    const nx::utils::stree::ResourceContainer& authInfo,
    const api::Storage& storage,
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    auto accountOwner = getAccountEmail(authInfo);
    if (accountOwner == storage.owner)
        return handler(true);

    // Non-owner trying to access storage, check by user's access to a system in the storage

    if (storage.systems.empty())
        return handler(false);

    cloud::db::api::UserAuthorization userAuth;
    authInfo.get(cloud_db::Resource::httpMethod, &userAuth.requestMethod);
    authInfo.get(cloud_db::Resource::authorization, &userAuth.requestAuthorization);

    auto cdbClient = createCdbClient(std::move(handler));

    // TODO use batch request when it is implemented
    cdbClient->authProvider()->getSystemAccessLevel(
        storage.systems.front(),
        userAuth,
        [this, userAuth, cdbClient, systemId = storage.systems.front()](
            auto cdbResult,
            auto systemAccess)
        {
            auto context = removeCdbClient(cdbClient);
            if (cdbResult != db::api::ResultCode::ok)
            {
                NX_ERROR(this, "cloud_db: getSystemAccess: systemId = %1, userAuth = %2"
                    " failed with result code: %3",
                    systemId, toString(userAuth), db::api::toString(cdbResult));
            }

            return context.handler(systemAccess.accessRole > db::api::SystemAccessRole::disabled);
        });
}

std::string AccessManager::getAccountEmail(const nx::utils::stree::ResourceContainer& authInfo) const
{
    std::string accountOwner;
    authInfo.get(cloud_db::Resource::accountEmail, &accountOwner);
    return accountOwner;
}

bool AccessManager::isStorageOwner(
    const nx::utils::stree::ResourceContainer& authInfo,
    const api::Storage& storage) const
{
    return getAccountEmail(authInfo) == storage.owner;
}

db::api::CdbClient* AccessManager::createCdbClient(
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    auto cdbClient = std::make_unique<db::api::CdbClient>();
    cdbClient->setCloudUrl(m_settings.url.toStdString());
    auto cdbClientPtr = cdbClient.get();

    ReadStorageContext readStorageContext;
    readStorageContext.cdbClient = std::move(cdbClient);
    readStorageContext.handler = std::move(handler);

    QnMutexLocker lock(&m_mutex);
    m_cdbRequests.emplace(cdbClientPtr, std::move(readStorageContext));

    return cdbClientPtr;
}

AccessManager::ReadStorageContext AccessManager::removeCdbClient(
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