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
    int64_t timestamp;
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
    return nx::Buffer();
}

void CloudUserInfoPool::userInfoChanged(
    int64_t timestamp,
    const nx::Buffer& userName,
    const nx::Buffer& cloudNonce,
    const nx::Buffer& partialResponse)
{
}

void CloudUserInfoPool::userInfoRemoved(const nx::Buffer& userName)
{
}

namespace detail {

bool deserialize(
    const QString& serializedValue,
    int64_t* timestamp,
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
    *timestamp = (int64_t)timestampVal.toDouble();

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
