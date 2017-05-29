#include <QJsonDocument>
#include <QJsonObject>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <nx/utils/log/log.h>
#include "cloud_user_info_pool.h"


static const QString kCloudAuthInfoKey = lit("CloudAuthInfo");

// CloudUserInfoPoolSupplier
CloudUserInfoPoolSupplier::CloudUserInfoPoolSupplier(QnCommonModule* commonModule)
    :
    QnCommonModuleAware(commonModule),
    m_pool(nullptr)
{
    for (const auto& userResource: resourcePool()->getResources<QnUserResource>())
    {
        auto value = userResource->getProperty(kCloudAuthInfoKey);
        if (value.isEmpty())
            continue;
        reportInfoChanged(userResource->getName(), value);
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
    connect(
        resourcePool(),
        &QnResourcePool::resourceAdded,
        this,
        &CloudUserInfoPoolSupplier::onNewResource, Qt::QueuedConnection);
    connect(
        resourcePool(),
        &QnResourcePool::resourceRemoved,
        this,
        &CloudUserInfoPoolSupplier::onRemoveResource,
        Qt::QueuedConnection);
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

            NX_LOG(lit("[CloudUserInfo] CloudAuthInfo changed for user %1. New value: %2")
                .arg(resource->getName())
                .arg(resource->getProperty(key)), cl_logDEBUG2);

            const auto propValue = resource->getProperty(key);
            if (propValue.isEmpty())
            {
                NX_LOG(lit("[CloudUserInfo] User %1. CloudAuthInfo removed.")
                    .arg(resource->getName()), cl_logDEBUG1);
                m_pool->userInfoRemoved(resource->getName().toUtf8().toLower());
                return;
            }

            reportInfoChanged(resource->getName(), propValue);
        });
}

void CloudUserInfoPoolSupplier::reportInfoChanged(
    const QString& userName,
    const QString& serializedValue)
{
    uint64_t timestamp;
    nx::Buffer cloudNonce;
    nx::Buffer partialResponse;

    if (!detail::deserialize(serializedValue, &timestamp, &cloudNonce, &partialResponse))
    {
        NX_LOG(lit("[CloudUserInfo] User %1. Deserialization failed")
            .arg(userName), cl_logDEBUG1);
        return;
    }

    m_pool->userInfoChanged(
        timestamp,
        userName.toUtf8().toLower(),
        cloudNonce,
        partialResponse);
}

void CloudUserInfoPoolSupplier::onRemoveResource(const QnResourcePtr& resource)
{
    m_pool->userInfoRemoved(resource->getName().toUtf8().toLower());
}

// CloudUserInfoPool
CloudUserInfoPool::CloudUserInfoPool(std::unique_ptr<AbstractCloudUserInfoPoolSupplier> supplier):
    m_supplier(std::move(supplier))
{
    m_supplier->setPool(this);
}

bool CloudUserInfoPool::authenticate(const nx_http::header::Authorization& authHeader) const
{
    return false;
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
    const nx::Buffer& partialResponse)
{
    QnMutexLocker lock(&m_mutex);
    updateNameToNonces(userName, cloudNonce);
    updateNonceToTs(cloudNonce, timestamp);
    updateUserNonceToResponse(userName, cloudNonce, partialResponse);
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
        NX_ASSERT(m_nonceToTs[cloudNonce] == timestamp);
}

void CloudUserInfoPool::updateUserNonceToResponse(
    const nx::Buffer& userName,
    const nx::Buffer& cloudNonce,
    const nx::Buffer& partialResponse)
{
    auto result = m_userNonceToResponse.emplace(detail::UserNonce(userName, cloudNonce), partialResponse);
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
            it = m_userNonceToResponse.erase(it);
            decUserCountByNonce(it->first.cloudNonce);
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

namespace detail {

bool deserialize(
    const QString& serializedValue,
    uint64_t* timestamp,
    nx::Buffer* cloudNonce,
    nx::Buffer* partialResponse)
{
    auto jsonObj = QJsonDocument::fromJson(serializedValue.toUtf8()).object();
    if (jsonObj.isEmpty())
    {
        NX_LOG("[CloudUserInfo, deserialize] Json object is empty.", cl_logERROR);
        return false;
    }

    const QString kTimestampKey = lit("timestamp");
    const QString kCloudNonceKey = lit("cloudNonce");
    const QString kPartialResponseKey = lit("partialResponse");

    auto timestampVal = jsonObj[kTimestampKey];
    if (timestampVal.isUndefined() || !timestampVal.isDouble())
    {
        NX_LOG("[CloudUserInfo, deserialize] timestamp is undefined or wrong type.", cl_logERROR);
        return false;
    }
    *timestamp = (uint64_t)timestampVal.toDouble();

    auto cloudNonceVal = jsonObj[kCloudNonceKey];
    if (cloudNonceVal.isUndefined() || !cloudNonceVal.isString())
    {
        NX_LOG("[CloudUserInfo, deserialize] cloud nonce is undefined or wrong type.", cl_logERROR);
        return false;
    }
    *cloudNonce = cloudNonceVal.toString().toUtf8();

    auto partialResponseVal = jsonObj[kPartialResponseKey];
    if (partialResponseVal.isUndefined() || !partialResponseVal.isString())
    {
        NX_LOG("[CloudUserInfo, deserialize] partial response is undefined or wrong type.", cl_logERROR);
        return false;
    }
    *partialResponse = partialResponseVal.toString().toUtf8();

    return true;
}

}
