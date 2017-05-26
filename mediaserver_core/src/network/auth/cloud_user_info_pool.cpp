#include <common/common_module.h>
#include "cloud_user_info_pool.h"


namespace detail {

static const QString kCloudAuthInfoKey = lit("CloudAuthInfo");

// CloudUserInfoPoolSupplier
CloudUserInfoPoolSupplier::CloudUserInfoPoolSupplier(
    QnCommonModule* commonModule,
    AbstractCloudUserInfoPoolSupplierHandler* handler)
    :
    QnCommonModuleAware(commonModule),
    m_handler(handler)
{
    for (const auto& userResource: resourcePool()->getResources<QnUserResource>())
    {
        auto value = userResource->get(kCloudAuthInfoKey);
        if (value.isEmpty())
            continue;
        reportInfoChanged(value);
    }
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
        resource,
        &QnResource::propertyChanged,
        this,
        [this](const QnResourcePtr& resource, const QString& key)
        {
            if (key != lit(kCloudAuthInfoKey))
                return;

            NX_LOG(lit("[CloudUserInfo] CloudAuthInfo changed for user %1. New value: %2")
                .arg(resource->getName().toUtf8())
                .arg(resource->getProperty(key)), cl_logDEBUG2);

            const auto propValue = resource->getProperty(key);
            if (propValue.isEmpty())
            {
                NX_LOG(lit("[CloudUserInfo] User %1. CloudAuthInfo removed.")
                    .arg(resource->getName().toUtf8()), cl_logDEBUG1);
                m_handler->userInfoRemoved(resource->getName().toUtf8().toLower());
                return;
            }

            reportInfoChanged(propValue);
        });
}

void CloudUserInfoPoolSupplier::reportInfoChanged(const QByteArray& serializedValue)
{
    int64_t timestamp;
    nx::Buffer cloudNonce;

    if (!deserialize(serializedValue, &timestamp, cloudNonce))
    {
        NX_LOG(lit("[CloudUserInfo] User %1. Deserialization failed")
            .arg(resource->getName().toUtf8()), cl_logDEBUG1);
        return;
    }

    m_handler->userInfoChaged(
        timestmap,
        resource->getName().toUtf8().toLower(),
        cloudNonce);
}

void CloudUserInfoPoolSupplier::onRemoveResource(const QnResourcePtr& resource)
{
    m_handler->userInfoRemoved(resource->getName().toUtf8().toLower());
}

} // namespace detail

// CloudUserInfoPool
bool CloudUserInfoPoolSupplier::authenticate(const nx::http::header::Authorization& authHeader) const
{
}

boost::optional<nx::Buffer> CloudUserInfoPoolSupplier::newestMostCommonNonce() const
{
}

