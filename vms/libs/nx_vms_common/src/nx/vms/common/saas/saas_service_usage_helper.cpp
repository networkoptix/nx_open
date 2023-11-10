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

using namespace nx::vms::api;

namespace {

int getMegapixels(const auto& camera)
{
    int megapixels = camera->cameraMediaCapability().maxResolution.megaPixels();
    if (megapixels == 0)
    {
        // In case of device don't provide megapixel information then it requires license
        // without megapixels restriction.
        megapixels = SaasCloudStorageParameters::kUnlimitedResolution;
    }
    return megapixels;
}

} // namespace

namespace nx::vms::common::saas {

// --------------------------- CloudServiceUsageHelper -------------------

CloudServiceUsageHelper::CloudServiceUsageHelper(
    SystemContext* context,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(context)
{
}

void CloudServiceUsageHelper::sortCameras(QnVirtualCameraResourceList* inOutCameras)
{
    std::sort(
        inOutCameras->begin(), inOutCameras->end(),
        [](const auto& left, const auto& right)
        {
            const int leftMegapixels = getMegapixels(left);
            const int rightMegapixels = getMegapixels(right);
            if (leftMegapixels != rightMegapixels)
                return leftMegapixels > rightMegapixels;
            return left->getId() < right->getId();
        });
}

QnVirtualCameraResourceList CloudServiceUsageHelper::getAllCameras() const
{
    auto cameras = systemContext()->resourcePool()->getAllCameras(
        QnResourcePtr(), /*ignoreDesktopCameras*/ true);
    sortCameras(&cameras);
    return cameras;
}

// --------------------------- IntegrationServiceUsageHelper -------------------

IntegrationServiceUsageHelper::IntegrationServiceUsageHelper(
    SystemContext* context,
    QObject* parent)
    :
    CloudServiceUsageHelper(context, parent)
{
}

void IntegrationServiceUsageHelper::invalidateCache()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_cache.reset();
}

void IntegrationServiceUsageHelper::updateCacheUnsafe() const
{
    m_cache = std::map<QString, LicenseSummaryDataEx>();

    // Update integration info
    const auto saasData = m_context->saasServiceManager()->data();

    for (const auto& [serviceId, integration]: m_context->saasServiceManager()->analyticsIntegrations())
    {
        auto& cacheData = (*m_cache)[integration.integrationId];
        cacheData.total += integration.totalChannelNumber;
        if (saasData.state == SaasState::active)
            cacheData.available += integration.totalChannelNumber;
    }

    for (const auto& camera: getAllCameras())
    {
        for (const auto& engineId: camera->userEnabledAnalyticsEngines())
        {
            if (auto r = resourcePool()->getResourceById<AnalyticsEngineResource>(engineId))
            {
                const auto manifest = r->plugin()->manifest();
                if (manifest.isLicenseRequired)
                {
                    auto& value = (*m_cache)[manifest.id];
                    ++value.inUse;
                    if (value.inUse > value.available)
                        value.exceedDevices.insert(camera->getId());
                }
            }
        }
    }
}

LicenseSummaryDataEx IntegrationServiceUsageHelper::info(const QString& integrationId)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_cache)
        updateCacheUnsafe();
    const auto it = m_cache->find(integrationId);
    return it != m_cache->end() ? it->second : LicenseSummaryDataEx();
}

std::map<QString, LicenseSummaryDataEx> IntegrationServiceUsageHelper::allInfo() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_cache)
        updateCacheUnsafe();

    return *m_cache;
}

void IntegrationServiceUsageHelper::proposeChange(
    const QnUuid& resourceId, const QSet<QnUuid>& integrations)
{
    std::vector<Propose> data;
    data.push_back(Propose{resourceId, integrations});
    proposeChange(data);
}

void IntegrationServiceUsageHelper::proposeChange(const std::vector<Propose>& data)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    updateCacheUnsafe();

    for (const auto& propose: data)
    {
        const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(propose.resourceId);
        if (!camera)
            continue;
        for (const auto& id: camera->userEnabledAnalyticsEngines())
        {
            if (auto r = resourcePool()->getResourceById<AnalyticsEngineResource>(id))
            {
                const auto manifest = r->plugin()->manifest();
                if (manifest.isLicenseRequired)
                {
                    auto& value = (*m_cache)[manifest.id];
                    --value.inUse;
                }
            }
        }
        for (const auto& id: propose.integrations)
        {
            if (auto r = resourcePool()->getResourceById<AnalyticsEngineResource>(id))
            {
                const auto manifest = r->plugin()->manifest();
                if (manifest.isLicenseRequired)
                {
                    auto& value = (*m_cache)[manifest.id];
                    ++value.inUse;
                }
            }
        }
    }
}

bool IntegrationServiceUsageHelper::isOverflow() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_cache)
        updateCacheUnsafe();

    for (auto itr = m_cache->begin(); itr != m_cache->end(); ++itr)
    {
        if (itr->second.inUse > itr->second.available)
            return true;
    }
    return false;
}

std::map<QnUuid, std::set<QString>> IntegrationServiceUsageHelper::camerasByService() const
{
    std::map<QnUuid, std::set<QString>> result;

    // Update integration info
    QMap<QString, QnUuid> serviceByIntegration;
    for (const auto& [serviceId, integration]: m_context->saasServiceManager()->analyticsIntegrations())
        serviceByIntegration[integration.integrationId] = serviceId;

    for (const auto& camera: getAllCameras())
    {
        const auto& engines = camera->userEnabledAnalyticsEngines();
        for (const auto& engineId: engines)
        {
            if (auto r = resourcePool()->getResourceById<AnalyticsEngineResource>(engineId))
            {
                const auto manufest = r->plugin()->manifest();
                if (manufest.isLicenseRequired)
                {
                    const auto& serviceId = serviceByIntegration.value(manufest.id);
                    result[serviceId].insert(camera->getPhysicalId());
                }
            }
        }
    }
    return result;
}

// --------------------------- ServiceInfo --------------------------------------

bool CloudStorageServiceUsageHelper::ServiceInfo::operator<(
    const CloudStorageServiceUsageHelper::ServiceInfo& other) const
{
    if (megapixels != other.megapixels)
        return megapixels < other.megapixels;
    return serviceId < other.serviceId;
}

// --------------------------- CloudStorageServiceUsageHelper -------------------

CloudStorageServiceUsageHelper::CloudStorageServiceUsageHelper(
    SystemContext* context,
    QObject* parent)
    :
    CloudServiceUsageHelper(context, parent)
{
}

void CloudStorageServiceUsageHelper::updateCache()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    updateCacheUnsafe();
}

void CloudStorageServiceUsageHelper::invalidateCache()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_cache.reset();
}

void CloudStorageServiceUsageHelper::calculateAvailableUnsafe() const
{
    auto& cache = *m_cache;
    const auto saasData = m_context->saasServiceManager()->data();
    auto cloudStorageData = systemContext()->saasServiceManager()->cloudStorageData();
    for (const auto& [id, parameters]: cloudStorageData)
    {
        auto& cacheData = cache[{parameters.maxResolutionMp, id}];
        cacheData.total += parameters.totalChannelNumber;
        if (saasData.state == SaasState::active)
            cacheData.available += parameters.totalChannelNumber;
    }
}

void CloudStorageServiceUsageHelper::updateCacheUnsafe() const
{
    m_cache = std::map<ServiceInfo, LicenseSummaryData>();
    calculateAvailableUnsafe();
    processUsedDevicesUnsafe(getAllCameras());
}

void CloudStorageServiceUsageHelper::processUsedDevicesUnsafe(
    const QnVirtualCameraResourceList& cameras) const
{
    for (const auto& camera: cameras)
    {
        if (camera->isBackupEnabled())
            countCameraAsUsedUnsafe(camera);
    }
    borrowUsages();
}

void CloudStorageServiceUsageHelper::countCameraAsUsedUnsafe(
    const QnVirtualCameraResourcePtr& camera) const
{
    const int megapixels = getMegapixels(camera);
    ServiceInfo key{megapixels, QnUuid()};
    auto it = m_cache->lower_bound(key);
    if (it == m_cache->end())
        it = m_cache->emplace(key, LicenseSummaryData()).first;
    ++it->second.inUse;
}

void CloudStorageServiceUsageHelper::setUsedDevices(const QSet<QnUuid>& devices)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_cache = std::map<ServiceInfo, LicenseSummaryData>();
    calculateAvailableUnsafe();

    QnVirtualCameraResourceList cameras;
    for (const auto& id: devices)
    {
        const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id);
        if (camera)
            cameras.push_back(camera);
    }
    sortCameras(&cameras);
    processUsedDevicesUnsafe(cameras);
}

void CloudStorageServiceUsageHelper::proposeChange(
    const std::set<QnUuid>& devicesToAdd,
    const std::set<QnUuid>& devicesToRemove)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_cache = std::map<ServiceInfo, LicenseSummaryData>();
    calculateAvailableUnsafe();

    auto cameras = systemContext()->resourcePool()->getAllCameras(
        /*server*/ QnResourcePtr(),
        /*ignoreDesktopCameras*/ true);

    std::map<QnUuid, QnVirtualCameraResourcePtr> backupEnabledCamerasById;
    for (const auto& camera: cameras)
    {
        if (camera->isBackupEnabled())
            backupEnabledCamerasById.insert({camera->getId(), camera});
    }

    for (const auto& id: devicesToAdd)
    {
        if (devicesToRemove.contains(id))
            continue;
        if (const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id))
            backupEnabledCamerasById.insert({camera->getId(), camera});
    };

    for (const auto& id: devicesToRemove)
    {
        if (devicesToAdd.contains(id))
            continue;
        backupEnabledCamerasById.erase(id);
    };

    cameras.clear();
    for (const auto& [_, camera]: backupEnabledCamerasById)
        cameras.push_back(camera);

    sortCameras(&cameras);
    for (const auto& camera: cameras)
        countCameraAsUsedUnsafe(camera);

    borrowUsages();
}

void CloudStorageServiceUsageHelper::borrowUsages() const
{
    for (auto it = m_cache->begin(); it != m_cache->end(); ++it)
    {
        auto& data = it->second;
        int lack = data.inUse - data.available;
        for (auto itNext = std::next(it); lack > 0 && itNext != m_cache->end(); ++itNext)
        {
            auto& dataNext = itNext->second;
            const int free = std::min(lack, dataNext.available - dataNext.inUse);
            if (free > 0)
            {
                dataNext.inUse += free;
                data.inUse -= free;
                lack -= free;
            }
        }
    }
}

bool CloudStorageServiceUsageHelper::isOverflow() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_cache)
        updateCacheUnsafe();
    return std::any_of(m_cache->begin(), m_cache->end(),
        [](const auto& value)
        {
            return value.second.inUse > value.second.available;
        });
}

std::map<int, LicenseSummaryData> CloudStorageServiceUsageHelper::allInfoByMegapixels() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_cache)
        updateCacheUnsafe();
    std::map<int, LicenseSummaryData> result;
    for (const auto& [key, value]: *m_cache)
        result.emplace(key.megapixels, value);
    return result;
}

LicenseSummaryData CloudStorageServiceUsageHelper::allInfoForResolution(int megapixels) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_cache)
        updateCacheUnsafe();

    if (megapixels == 0)
        megapixels = SaasCloudStorageParameters::kUnlimitedResolution;

    LicenseSummaryData result;
    for (const auto& [key, value]: *m_cache)
    {
        if (key.megapixels < megapixels)
            continue;
        result.available += value.available;
        result.inUse += value.inUse;
        result.total += value.total;
    }
    return result;
}

std::map<QnUuid, LicenseSummaryData> CloudStorageServiceUsageHelper::allInfoByService() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_cache)
        updateCacheUnsafe();
    std::map<QnUuid, LicenseSummaryData> result;
    for (const auto& [key, value]: *m_cache)
        result.emplace(key.serviceId, value);
    return result;
}

std::map<QnUuid, QnUuid> CloudStorageServiceUsageHelper::servicesByCameras() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    std::map<QnUuid, QnUuid> result;
    using Data = std::pair<QnUuid, int>; //< ServiceId and channels.
    const auto saasData = m_context->saasServiceManager()->data();
    std::multimap<int, Data> availableChannels;
    if (saasData.state == SaasState::active)
    {
        auto cloudStorageData = systemContext()->saasServiceManager()->cloudStorageData();
        for (const auto& [id, parameters]: cloudStorageData)
        {
            if (parameters.totalChannelNumber > 0)
            {
                availableChannels.emplace(
                    parameters.maxResolutionMp, Data{id, parameters.totalChannelNumber});
            }
        }
    }

    for (const auto& camera: getAllCameras())
    {
        if (!camera->isBackupEnabled())
            continue;
        // Spend a service on the camera.
        const int megapixels = getMegapixels(camera);
        auto it = availableChannels.lower_bound(megapixels);
        if (it == availableChannels.end())
        {
            result.emplace(camera->getId(), QnUuid()); //< No service available.
            continue;
        }

        // If all channels of the current service is spend, go next service
        for (auto it2 = it; it2 != availableChannels.end(); ++it2)
        {
            Data& data = it2->second;
            if (data.second > 0)
            {
                it = it2;
                break;
            }
        }

        Data& data = it->second;
        result.emplace(camera->getId(), data.first);
        --data.second;
    }

    return result;
}

// --------------------------- LocalRecordingUsageHelper -------------------

LocalRecordingUsageHelper::LocalRecordingUsageHelper(
    SystemContext* context,
    QObject* parent)
    :
    CloudServiceUsageHelper(context, parent)
{
}

std::map<QnUuid, std::set<QString>> LocalRecordingUsageHelper::camerasByService() const
{
    std::map<QnUuid, std::set<QString>> result;

    // Update integration info
    std::map<QnUuid, int> channelsByService;
    for (const auto& [serviceId, params]: m_context->saasServiceManager()->localRecording())
    {
        if (params.totalChannelNumber > 0)
            channelsByService[serviceId] = params.totalChannelNumber;
    }

    for (const auto& camera: getAllCameras())
    {
        if (!camera->isScheduleEnabled())
            continue;
        QnUuid serviceId;
        if (!channelsByService.empty())
        {
            const auto it = channelsByService.begin();
            serviceId = it->first;
            if (--it->second == 0)
                channelsByService.erase(it);
        }
        result[serviceId].insert(camera->getPhysicalId());
    }
    return result;
}

} // nx::vms::common::saas
