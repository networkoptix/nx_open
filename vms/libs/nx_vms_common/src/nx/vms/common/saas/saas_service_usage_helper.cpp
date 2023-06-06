// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_service_usage_helper.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <licensing/license.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/system_context.h>

namespace {

int getMegapixels(const auto& camera)
{
    int megapixels = camera->cameraMediaCapability().maxResolution.megaPixels();
    if (megapixels == 0)
    {
        // In case of device don't provide megapixel information then it requires license
        // without megapixels restriction.
        megapixels = std::numeric_limits<int>::max();
    }
    return megapixels;
}

} // namespace

namespace nx::vms::common::saas {

CloudServiceUsageHelper::CloudServiceUsageHelper(QObject* parent):
    QObject(parent)
{
}

IntegrationServiceUsageHelper::IntegrationServiceUsageHelper(SystemContext* context, QObject* parent):
    CloudServiceUsageHelper(parent),
    SystemContextAware(context)
{
    connect(
        context->saasServiceManager(), &saas::ServiceManager::dataChanged,
        this, &IntegrationServiceUsageHelper::invalidateCache);
}

void IntegrationServiceUsageHelper::invalidateCache()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_cache.reset();
}

void IntegrationServiceUsageHelper::updateCacheUnsafe()
{
    m_cache = QMap<QnUuid, nx::vms::api::LicenseSummaryData>();

    // Update integration info
    using namespace nx::vms::api;
    const auto saasData = m_context->saasServiceManager()->data();

    for (const auto& [integrationId, integration]: m_context->saasServiceManager()->analyticsIntegrations())
    {
        auto& cacheData = (*m_cache)[integrationId];
        cacheData.total += integration.totalChannelNumber;
        if (saasData.state == SaasState::active)
            cacheData.available += integration.totalChannelNumber;
    }

    for (const auto& camera: resourcePool()->getAllCameras())
    {
        for (const auto& engineId: camera->userEnabledAnalyticsEngines())
        {
            if (auto r = resourcePool()->getResourceById<AnalyticsEngineResource>(engineId))
            {
                if (r->plugin()->manifest().isLicenseRequired)
                {
                    auto& value = (*m_cache)[r->getId()];
                    ++value.inUse;
                }
            }
        }
    }
}

nx::vms::api::LicenseSummaryData IntegrationServiceUsageHelper::info(const QnUuid& id)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_cache)
        updateCacheUnsafe();

    return m_cache->value(id);
}

QMap<QnUuid, nx::vms::api::LicenseSummaryData> IntegrationServiceUsageHelper::allInfo()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_cache)
        updateCacheUnsafe();

    return *m_cache;
}

void IntegrationServiceUsageHelper::proposeChange(const QnUuid& resourceId, const QSet<QnUuid>& integrations)
{
    if (const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(resourceId))
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        updateCacheUnsafe();
        for (const auto& id: camera->userEnabledAnalyticsEngines())
        {
            if (auto r = resourcePool()->getResourceById<AnalyticsEngineResource>(id))
            {
                if (r->plugin()->manifest().isLicenseRequired)
                {
                    auto& value = (*m_cache)[r->getId()];
                    --value.inUse;
                }
            }
        }
        for (const auto& id: integrations)
        {
            if (auto r = resourcePool()->getResourceById<AnalyticsEngineResource>(id))
            {
                if (r->plugin()->manifest().isLicenseRequired)
                {
                    auto& value = (*m_cache)[r->getId()];
                    ++value.inUse;
                }
            }
        }
    }
}

bool IntegrationServiceUsageHelper::isOverflow() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    for (auto itr = m_cache->begin(); itr != m_cache->end(); ++itr)
    {
        if (itr.value().inUse > itr.value().available)
            return true;
    }
    return false;
}

CloudStorageServiceUsageHelper::CloudStorageServiceUsageHelper(
    SystemContext* context,
    QObject* parent)
    :
    CloudServiceUsageHelper(parent),
    SystemContextAware(context)
{
}

void CloudStorageServiceUsageHelper::updateCache()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    updateCacheUnsafe();
}

void CloudStorageServiceUsageHelper::updateCacheUnsafe()
{
    using namespace nx::vms::api;

    m_cache = std::map<int, nx::vms::api::LicenseSummaryData>();
    auto& cache = *m_cache;

    const auto saasData = m_context->saasServiceManager()->data();
    auto cloudStorageData = systemContext()->saasServiceManager()->cloudStorageData();
    for (const auto& [_, parameters]: cloudStorageData)
    {
        auto& cacheData = cache[parameters.maxResolution];
        cacheData.total += parameters.totalChannelNumber;
        if (saasData.state == SaasState::active)
            cacheData.available += parameters.totalChannelNumber;
    }

    auto cameras = systemContext()->resourcePool()->getAllCameras();
    for (const auto& camera: cameras)
    {
        if (camera->isBackupEnabled())
        {
            const int megapixels = getMegapixels(camera);
            auto it = m_cache->lower_bound(megapixels);
            if (it == m_cache->end())
                it = m_cache->emplace(megapixels, LicenseSummaryData()).first;
            ++it->second.inUse;
        }
    }
}

void CloudStorageServiceUsageHelper::proposeChange(const QSet<QnUuid>& devices)
{
    using namespace nx::vms::api;

    NX_MUTEX_LOCKER lock(&m_mutex);
    updateCacheUnsafe();

    for (auto& [_, info] : *m_cache)
        info.inUse = 0;

    for (const auto& id: devices)
    {
        const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id);
        if (!camera)
            continue;

        const int megapixels = getMegapixels(camera);
        auto it = m_cache->lower_bound(megapixels);
        if (it == m_cache->end())
            it = m_cache->emplace(megapixels, LicenseSummaryData()).first;
        ++it->second.inUse;
    }

    borrowUsages();
}

void CloudStorageServiceUsageHelper::borrowUsages()
{
    for (auto it = m_cache->begin(); it != m_cache->end(); ++it)
    {
        auto& data = it->second;
        if (data.inUse > data.available)
        {
            for (auto itNext = std::next(it); itNext != m_cache->end(); ++itNext)
            {
                auto& dataNext = itNext->second;
                if (dataNext.inUse < dataNext.available)
                {
                    ++dataNext.inUse;
                    --data.inUse;
                }
            }
        }
    }
}

bool CloudStorageServiceUsageHelper::isOverflow() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_cache)
        return false;
    return std::any_of(m_cache->begin(), m_cache->end(),
        [](const auto& value)
        {
            return value.second.inUse > value.second.available;
        });
}

std::map<int, nx::vms::api::LicenseSummaryData> CloudStorageServiceUsageHelper::allInfo() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_cache)
        return std::map<int, nx::vms::api::LicenseSummaryData>();
    return *m_cache;
}

} // nx::vms::common::saas
