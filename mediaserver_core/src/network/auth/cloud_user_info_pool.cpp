#include <algorithm>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <nx/utils/log/log.h>
#include <nx/network/http/auth_tools.h>
#include <nx/cloud/cdb/api/auth_provider.h>
#include <nx/fusion/serialization/json.h>
#include "cloud_user_info_pool.h"
#include "cdb_nonce_fetcher.h"


static const QString kCloudAuthInfoKey = nx::cdb::api::kVmsUserAuthInfoAttributeName;

// CloudUserInfoPoolSupplier
CloudUserInfoPoolSupplier::CloudUserInfoPoolSupplier(QnCommonModule* commonModule)
    :
    QnCommonModuleAware(commonModule),
    m_pool(nullptr)
{
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
    auto userResource = resource.dynamicCast<QnUserResource>();
    if (!userResource)
        return;

    auto cloudAuthInfoProp = userResource->getProperty(kCloudAuthInfoKey);
    if (!cloudAuthInfoProp.isEmpty())
        reportInfoChanged(resource->getName(), cloudAuthInfoProp);

    Qn::directConnect(
        resource.data(),
        &QnResource::propertyChanged,
        this,
        [this](const QnResourcePtr& resource, const QString& key)
        {
            if (key != kCloudAuthInfoKey)
                return;

            NX_LOGX(lit("Changed for user %1. New value: %2")
                .arg(resource->getName())
                .arg(resource->getProperty(key)), cl_logDEBUG2);

            reportInfoChanged(resource->getName(), resource->getProperty(key));
        });
}

void CloudUserInfoPoolSupplier::reportInfoChanged(
    const QString& userName,
    const QString& serializedValue)
{
    if (serializedValue.isEmpty())
    {
        NX_DEBUG(this, lit("User %1. Received empty cloud auth info").arg(userName));
        return;
    }

    nx::cdb::api::AuthInfo authInfo;
    bool deserializeResult = QJson::deserialize(serializedValue, &authInfo);
    NX_ASSERT(deserializeResult);

    if (!deserializeResult)
    {
        NX_LOGX(lit("User %1. Deserialization failed")
            .arg(userName), cl_logDEBUG1);
        return;
    }

    m_pool->userInfoChanged(userName.toUtf8().toLower(), authInfo);
}

void CloudUserInfoPoolSupplier::onRemoveResource(const QnResourcePtr& resource)
{
    auto userResource = resource.dynamicCast<QnUserResource>();
    if (!userResource)
        return;

    NX_LOGX(lit("User %1 removed. Clearing related data.")
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
        NX_LOGX(lm("parseCloudNonce() failed. User: %1, nonce: %2")
            .arg(userName)
            .arg(nonce), cl_logERROR);
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
    for (const auto& rec: m_cloudUserInfoRecordList)
    {
        if (rec.userName == userName && rec.nonce == cloudNonce)
            return rec.intermediateResponse;
    }

    return boost::none;
}

boost::optional<nx::Buffer> CloudUserInfoPool::newestMostCommonNonce() const
{
    QnMutexLocker lock(&m_mutex);
    if (!m_nonce.isNull())
        return m_nonce;

    NX_VERBOSE(this, lm("Could not find nonce"));
    return boost::none;
}

void CloudUserInfoPool::userInfoChanged(
    const nx::Buffer& userName,
    const nx::cdb::api::AuthInfo& authInfo)
{
    NX_VERBOSE(this, lm("User info changed for user: %2").arg(userName));
    QnMutexLocker lock(&m_mutex);

    removeInfoForUser(userName);

    for (const auto& authInfoRecord: authInfo.records)
    {
        m_cloudUserInfoRecordList.push_back(
            detail::CloudUserInfoRecord(
                authInfoRecord.expirationTime.time_since_epoch().count(),
                userName,
                nx::Buffer::fromStdString(authInfoRecord.nonce),
                nx::Buffer::fromStdString(authInfoRecord.intermediateResponse)));
    }

    updateNonce();
}

void CloudUserInfoPool::updateNonce()
{
    std::map<nx::Buffer, int> nonceToCount;
    std::map<nx::Buffer, uint64_t> nonceToMaxTs;

    for (const auto& rec: m_cloudUserInfoRecordList)
    {
        auto nonceToCountRes = nonceToCount.emplace(rec.nonce, 1);
        if (!nonceToCountRes.second)
            ++nonceToCountRes.first->second;

        auto nonceToMaxTsRes = nonceToMaxTs.emplace(rec.nonce, rec.timestamp);
        if (!nonceToMaxTsRes.second && nonceToMaxTs[rec.nonce] < rec.timestamp)
            nonceToMaxTs[rec.nonce] = rec.timestamp;
    }

    int maxCount = 0;
    for (const auto& nonceToCountPair: nonceToCount)
    {
        if (nonceToCountPair.second > maxCount)
        {
            m_nonce = nonceToCountPair.first;
            maxCount = nonceToCountPair.second;
        }
        else if (nonceToCountPair.second == maxCount
                 && nonceToMaxTs[nonceToCountPair.first] > nonceToMaxTs[m_nonce])
        {
            m_nonce = nonceToCountPair.first;
        }
    }
}

void CloudUserInfoPool::removeInfoForUser(const nx::Buffer& userName)
{
    m_cloudUserInfoRecordList.erase(
        std::remove_if(
            m_cloudUserInfoRecordList.begin(),
            m_cloudUserInfoRecordList.end(),
            [&userName](const detail::CloudUserInfoRecord& rec)
            {
                return rec.userName == userName;
            }), m_cloudUserInfoRecordList.end());
}

void CloudUserInfoPool::userInfoRemoved(const nx::Buffer& userName)
{
    QnMutexLocker lock(&m_mutex);
    NX_VERBOSE(this, lm("Removing cloud user info for user %1"));
    removeInfoForUser(userName);
}
