#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <nx/utils/log/log.h>
#include <nx/network/http/auth_tools.h>
#include <nx/cloud/cdb/client/data/auth_data.h>
#include <nx/cloud/cdb/api/auth_provider.h>
#include <nx/fusion/serialization/json.h>
#include "cloud_user_info_pool.h"
#include "cdb_nonce_fetcher.h"


static const QString kCloudAuthInfoKey = lit("cloudAuthInfo");

// CloudUserInfoPoolSupplier
CloudUserInfoPoolSupplier::CloudUserInfoPoolSupplier(QnCommonModule* commonModule)
    :
    QnCommonModuleAware(commonModule),
    m_pool(nullptr)
{
    for (const auto& userResource: resourcePool()->getResources<QnUserResource>())
    {
        onNewResource(userResource);
        reportInfoChanged(
            userResource->getName(),
            userResource->getProperty(kCloudAuthInfoKey));
    }
    connectToResourcePool();
}

void CloudUserInfoPoolSupplier::setPool(AbstractCloudUserInfoPool* pool)
{
    m_pool = pool;    
}

CloudUserInfoPoolSupplier::~CloudUserInfoPoolSupplier()
{
    directDisconnectAll();
}

void CloudUserInfoPoolSupplier::connectToResourcePool()
{
    Qn::directConnect(
        resourcePool(),
        &QnResourcePool::resourceAdded,
        this,
        &CloudUserInfoPoolSupplier::onNewResource);
    Qn::directConnect(
        resourcePool(),
        &QnResourcePool::resourceRemoved,
        this,
        &CloudUserInfoPoolSupplier::onRemoveResource);
}

void CloudUserInfoPoolSupplier::onNewResource(const QnResourcePtr& resource)
{
    Qn::directConnect(
        resource.data(),
        &QnResource::propertyChanged,
        this,
        [this](const QnResourcePtr& resource, const QString& key)
        {
            if (key != kCloudAuthInfoKey)
                return;

            NX_LOG(lit("[CloudUserInfo] Changed for user %1. New value: %2")
                .arg(resource->getName())
                .arg(resource->getProperty(key)), cl_logDEBUG2);

            reportInfoChanged(resource->getName(), resource->getProperty(key));
        });
}

void CloudUserInfoPoolSupplier::reportInfoChanged(
    const QString& userName,
    const QString& serializedValue)
{
    nx::cdb::api::AuthInfo authInfo;
    bool deserializeResult = QJson::deserialize(serializedValue, &authInfo);
    NX_ASSERT(deserializeResult);

    if (!deserializeResult)
    {
        NX_LOG(lit("[CloudUserInfo] User %1. Deserialization failed")
            .arg(userName), cl_logDEBUG1);
        return;
    }

    for (const auto& authInfoRecord : authInfo.records)
    {
        m_pool->userInfoChanged(
            authInfoRecord.expirationTime.time_since_epoch().count(),
            userName.toUtf8().toLower(),
            nx::Buffer::fromStdString(authInfoRecord.nonce),
            nx::Buffer::fromStdString(authInfoRecord.intermediateResponse));
    }
}

void CloudUserInfoPoolSupplier::onRemoveResource(const QnResourcePtr& resource)
{
    NX_LOG(lit("[CloudUserInfo] User %1 removed. Clearing related data.")
        .arg(resource->getName()), cl_logDEBUG1);
    m_pool->userInfoRemoved(resource->getName().toUtf8().toLower());
}

// CloudUserInfoPool
CloudUserInfoPool::CloudUserInfoPool(std::unique_ptr<AbstractCloudUserInfoPoolSupplier> supplier) :
    m_supplier(std::move(supplier))
{
    m_supplier->setPool(this);
}

bool CloudUserInfoPool::authenticate(
    const nx_http::Method::ValueType& method,
    const nx_http::header::Authorization& authHeader) const
{
    const QByteArray userName = authHeader.userid().toLower();
    const auto nonce = authHeader.digest->params["nonce"];
    nx::Buffer cloudNonce;
    nx::Buffer nonceTrailer;

    if (!CdbNonceFetcher::parseCloudNonce(nonce, &cloudNonce, &nonceTrailer))
    {
        NX_LOG(lit("[CloudUserInfo] parseCloudNonce() failed. User: %1, nonce: %2")
            .arg(QString::fromUtf8(userName))
            .arg(QString(nonce.toHex())), cl_logERROR);
        return false;
    }

    const auto intermediateResponse = intermediateResponseByUserNonce(userName, cloudNonce);
    if (!intermediateResponse)
        return false;

    const auto ha2 = nx_http::calcHa2(method, authHeader.digest->params["uri"]);
    const auto calculatedResponse = nx_http::calcResponseFromIntermediate(
        *intermediateResponse,
        cloudNonce.size(),
        nonceTrailer,
        ha2);

    if (authHeader.digest->params["response"] == calculatedResponse)
        return true;

    return false;
}

boost::optional<nx::Buffer> CloudUserInfoPool::intermediateResponseByUserNonce(
    const nx::Buffer& userName,
    const nx::Buffer& cloudNonce) const
{
    QnMutexLocker lock(&m_mutex);
    auto nameNonceIt = m_userNonceToResponse.find({ userName, cloudNonce });
    if (nameNonceIt == m_userNonceToResponse.cend())
    {
        NX_LOG(lit("[CloudUserInfo] Failed to find (user, cloudNonce) pair. User: %1, cloudNonce: %2")
            .arg(QString::fromUtf8(userName))
            .arg(QString(cloudNonce.toHex())), cl_logERROR);
        return boost::none;
    }
    return nameNonceIt->second;
}

boost::optional<nx::Buffer> CloudUserInfoPool::newestMostCommonNonce() const
{
    QnMutexLocker lock(&m_mutex);
    int maxUserCount = 0;
    auto resultIt = m_tsToNonceUserCount.rbegin();
    for (auto it = m_tsToNonceUserCount.rbegin(); it != m_tsToNonceUserCount.rend(); ++it)
    {
        if (it->second.userCount == m_userCount)
            return it->second.cloudNonce;

        if (it->second.userCount > maxUserCount)
        {
            maxUserCount = it->second.userCount;
            resultIt = it;
        }
    }

    return resultIt == m_tsToNonceUserCount.rend() ?
        boost::none :
        boost::optional<nx::Buffer>(resultIt->second.cloudNonce);
}

void CloudUserInfoPool::userInfoChanged(
    uint64_t timestamp,
    const nx::Buffer& userName,
    const nx::Buffer& cloudNonce,
    const nx::Buffer& intermediateResponse)
{
    QnMutexLocker lock(&m_mutex);
    updateNameToNonces(userName, cloudNonce);
    updateNonceToTs(cloudNonce, timestamp);
    updateUserNonceToResponse(userName, cloudNonce, intermediateResponse);
    updateTsToNonceCount(timestamp, cloudNonce);
}

void CloudUserInfoPool::updateNameToNonces(const nx::Buffer& userName, const nx::Buffer& cloudNonce)
{
    auto nameResult = m_nameToNonces.emplace(userName, std::unordered_set<nx::Buffer>{cloudNonce});
    if (!nameResult.second)
    {
        auto nonceResult = nameResult.first->second.emplace(cloudNonce);
        NX_ASSERT(nonceResult.second);
        return;
    }
    m_userCount++;
}

void CloudUserInfoPool::updateNonceToTs(const nx::Buffer& cloudNonce, uint64_t timestamp)
{
    auto nonceResult = m_nonceToTs.emplace(cloudNonce, timestamp);
    if (!nonceResult.second)
    {
        NX_ASSERT(m_nonceToTs[cloudNonce] == timestamp);
    }
}

void CloudUserInfoPool::updateUserNonceToResponse(
    const nx::Buffer& userName,
    const nx::Buffer& cloudNonce,
    const nx::Buffer& intermediateResponse)
{
    auto result = m_userNonceToResponse.emplace(detail::UserNonce(userName, cloudNonce), intermediateResponse);
    NX_ASSERT(result.second);
}

void CloudUserInfoPool::updateTsToNonceCount(uint64_t timestamp, const nx::Buffer& cloudNonce)
{
    auto result = m_tsToNonceUserCount.emplace(timestamp, detail::NonceUserCount(cloudNonce, 1));
    if (!result.second)
    {
        result.first->second.userCount++;
        NX_ASSERT(result.first->second.userCount <= m_userCount);
        if (result.first->second.userCount == m_userCount)
            cleanupOldInfo(result.first->first);
    }
}

void CloudUserInfoPool::cleanupOldInfo(uint64_t timestamp)
{
    for (auto tsIt = m_tsToNonceUserCount.cbegin(); tsIt->first < timestamp;)
    {
        cleanupByNonce(tsIt->second.cloudNonce);
        tsIt = m_tsToNonceUserCount.erase(tsIt);
    }
}

void CloudUserInfoPool::cleanupByNonce(const nx::Buffer& cloudNonce)
{
    for (auto it = m_userNonceToResponse.cbegin(); it != m_userNonceToResponse.cend();)
    {
        if (it->first.cloudNonce == cloudNonce)
            it = m_userNonceToResponse.erase(it);
        else
            ++it;
    }

    m_nonceToTs.erase(cloudNonce);

    for (auto it = m_nameToNonces.begin(); it != m_nameToNonces.end(); ++it)
        it->second.erase(cloudNonce);
}

void CloudUserInfoPool::userInfoRemoved(const nx::Buffer& userName)
{
    QnMutexLocker lock(&m_mutex);

    auto erased = m_nameToNonces.erase(userName);
    if (erased == 0)
        return;

    m_userCount--;
    for (auto it = m_userNonceToResponse.cbegin(); it != m_userNonceToResponse.cend();)
    {
        if (it->first.userName == userName)
        {
            decUserCountByNonce(it->first.cloudNonce);
            it = m_userNonceToResponse.erase(it);
        }
        else
            ++it;
    }
}

void CloudUserInfoPool::decUserCountByNonce(const nx::Buffer& cloudNonce)
{
    for (auto it = m_tsToNonceUserCount.begin(); it != m_tsToNonceUserCount.end();)
    {
        if (it->second.cloudNonce == cloudNonce)
        {
            if (--it->second.userCount == 0)
            {
                it = m_tsToNonceUserCount.erase(it);
                continue;
            }
        }
        ++it;
    }
}
