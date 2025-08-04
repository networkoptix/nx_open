// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_service_usage_helper.h"

#include <QtCore/QSet>
#include <QtCore/QTimer>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <licensing/license.h>
#include <nx/utils/uuid.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/license/usage_helper.h>

using namespace nx::vms::api;
using namespace nx::vms::common;

namespace {

int getMegapixels(const auto& camera)
{
    int megapixels = camera->backupMegapixels();
    if (megapixels == 0)
    {
        // In case of device don't provide megapixel information then it requires license
        // without megapixels restriction.
        megapixels = SaasCloudStorageParameters::kUnlimitedResolution;
    }
    return megapixels;
}

} // namespace

namespace nx::vms::license::saas {

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
    const auto saasData = saasServiceManager()->data();

    for (const auto& [serviceId, integration]: saasServiceManager()->analyticsIntegrations())
    {
        auto& cacheData = (*m_cache)[integration.integrationId];
        cacheData.serviceId = serviceId;
        cacheData.total += integration.totalChannelNumber;
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

std::tuple<int, int> IntegrationServiceUsageHelper::proposeChange(
    const nx::Uuid& resourceId, const std::set<nx::Uuid>& integrations)
{
    std::vector<Propose> data;
    data.push_back(Propose{resourceId, integrations});
    return proposeChange(data);
}

std::tuple<int, int> IntegrationServiceUsageHelper::proposeChange(const std::vector<Propose>& data)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    updateCacheUnsafe();

    int newLicensedEnginesAddedCount = 0;
    int oldLicensedEnginesRemovedCount = 0;

    for (const auto& propose: data)
    {
        const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(propose.resourceId);
        if (!camera)
            continue;

        QSet<nx::Uuid> alreadyEnabledEngines;
        for (const auto& id: camera->userEnabledAnalyticsEngines())
        {
            if (auto engineResource = resourcePool()->getResourceById<AnalyticsEngineResource>(id))
                alreadyEnabledEngines.insert(engineResource->getId());
        }

        QSet<nx::Uuid> proposedEngines;
        for (const auto& id: propose.integrations)
        {
            if (auto engineResource = resourcePool()->getResourceById<AnalyticsEngineResource>(id))
                proposedEngines.insert(engineResource->getId());
        }

        for (const auto& id: camera->userEnabledAnalyticsEngines())
        {
            if (auto alreadyEnabledEngineResource =
                    resourcePool()->getResourceById<AnalyticsEngineResource>(id))
            {
                const auto manifest = alreadyEnabledEngineResource->plugin()->manifest();
                if (manifest.isLicenseRequired)
                {
                    auto& value = (*m_cache)[manifest.id];

                    if (!proposedEngines.contains(alreadyEnabledEngineResource->getId()))
                    {
                        --value.inUse;
                        ++oldLicensedEnginesRemovedCount;
                    }
                }
            }
        }

        for (const auto& id: propose.integrations)
        {
            if (auto proposedEngineResource = resourcePool()->getResourceById<AnalyticsEngineResource>(id))
            {
                const auto manifest = proposedEngineResource->plugin()->manifest();
                if (manifest.isLicenseRequired)
                {
                    auto& value = (*m_cache)[manifest.id];
                    if (!alreadyEnabledEngines.contains(proposedEngineResource->getId()))
                    {
                        ++value.inUse;
                        ++newLicensedEnginesAddedCount;
                    }
                }
            }
        }
    }

    return std::make_tuple(newLicensedEnginesAddedCount,
        oldLicensedEnginesRemovedCount);
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

std::map<nx::Uuid, std::set<QString>> IntegrationServiceUsageHelper::camerasByService() const
{
    std::map<nx::Uuid, std::set<QString>> result;

    // Update integration info
    QMap<QString, nx::Uuid> serviceByIntegration;
    for (const auto& [serviceId, integration]: saasServiceManager()->analyticsIntegrations())
        serviceByIntegration[integration.integrationId] = serviceId;

    for (const auto& camera: getAllCameras())
    {
        const auto& engines = camera->userEnabledAnalyticsEngines();
        for (const auto& engineId: engines)
        {
            if (auto r = resourcePool()->getResourceById<AnalyticsEngineResource>(engineId))
            {
                const auto manifest = r->plugin()->manifest();
                if (manifest.isLicenseRequired)
                {
                    const auto& serviceId = serviceByIntegration.value(manifest.id);
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
    std::optional<std::chrono::milliseconds> cacheAutoUpdateInterval,
    QObject* parent)
    :
    CloudServiceUsageHelper(context, parent)
{
    if (!cacheAutoUpdateInterval.has_value())
        return;

    const auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &CloudStorageServiceUsageHelper::updateCache);
    timer->start(cacheAutoUpdateInterval.value());
}

void CloudStorageServiceUsageHelper::updateCache()
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        updateCacheUnsafe();
    }
    emit cacheUpdated();
}

void CloudStorageServiceUsageHelper::invalidateCache()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_cache.reset();
}

int CloudStorageServiceUsageHelper::licenseDeficit() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    int defecit = 0;
    if (!m_cache)
        updateCacheUnsafe();
    std::map<int, LicenseSummaryData> result;
    for (const auto& [key, value]: *m_cache)
        defecit += (value.inUse - value.available);

    return defecit;
}

void CloudStorageServiceUsageHelper::calculateAvailableUnsafe() const
{
    auto& cache = *m_cache;
    const auto saasData = saasServiceManager()->data();
    auto cloudStorageData = systemContext()->saasServiceManager()->cloudStorageData();
    for (const auto& [id, parameters]: cloudStorageData)
    {
        auto& cacheData = cache[{parameters.maxResolutionMp, id}];
        cacheData.total += parameters.totalChannelNumber;
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
    ServiceInfo key{megapixels, nx::Uuid()};
    auto it = m_cache->lower_bound(key);
    if (it == m_cache->end())
        it = m_cache->emplace(key, LicenseSummaryData()).first;
    ++it->second.inUse;
}

void CloudStorageServiceUsageHelper::setUsedDevices(const std::set<nx::Uuid>& devices)
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
    const std::set<nx::Uuid>& devicesToAdd,
    const std::set<nx::Uuid>& devicesToRemove)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_cache = std::map<ServiceInfo, LicenseSummaryData>();
    calculateAvailableUnsafe();

    auto cameras = systemContext()->resourcePool()->getAllCameras(
        /*server*/ QnResourcePtr(),
        /*ignoreDesktopCameras*/ true);

    std::map<nx::Uuid, QnVirtualCameraResourcePtr> backupEnabledCamerasById;
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

int CloudStorageServiceUsageHelper::overflowLicenseCount() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_cache)
        updateCacheUnsafe();
    int result = 0;
    for (const auto& [_, data]: *m_cache)
        result += std::max(0, data.inUse - data.available);
    return result;
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

std::map<nx::Uuid, LicenseSummaryData> CloudStorageServiceUsageHelper::allInfoByService() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_cache)
        updateCacheUnsafe();
    std::map<nx::Uuid, LicenseSummaryData> result;
    for (const auto& [key, value]: *m_cache)
        result.emplace(key.serviceId, value);
    return result;
}

std::map<QString, nx::Uuid> CloudStorageServiceUsageHelper::servicesByCameras() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    std::map<QString, nx::Uuid> result;
    using Data = std::pair<nx::Uuid, int>; //< ServiceId and channels.
    std::multimap<int, Data> availableChannels;
    if (saasServiceManager()->saasServiceOperational())
    {
        auto cloudStorageData = saasServiceManager()->cloudStorageData();
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
            result.emplace(camera->getPhysicalId(), nx::Uuid()); //< No service available.
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
        result.emplace(camera->getPhysicalId(), data.first);
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

std::map<nx::Uuid, std::set<QString>> LocalRecordingUsageHelper::cameraGroupsByService() const
{
    std::map<nx::Uuid, std::set<QString>> result;

    // Update integration info
    std::map<nx::Uuid, int> channelsByService; // key - serviceId, value - channel count.
    nx::Uuid defaultServiceId;
    for (const auto& [serviceId, params]: saasServiceManager()->localRecording())
    {
        channelsByService[serviceId] = params.totalChannelNumber;
        if (defaultServiceId.isNull() && params.totalChannelNumber > 0)
            defaultServiceId = serviceId;
    }
    if (defaultServiceId.isNull() && !channelsByService.empty())
        defaultServiceId = channelsByService.begin()->first;

    std::map<nx::Uuid, int>::iterator it = channelsByService.begin();

    std::set<QString> cameraGroups;
    for (const auto& camera: getAllCameras())
    {
        if (!camera->isScheduleEnabled() || camera->licenseType() == Qn::LicenseType::LC_Free)
            continue;
        cameraGroups.insert(camera->isSharingLicenseInGroup()
            ? camera->getGroupId()
            : camera->getPhysicalId());
    }

    for (const auto& cameraGroupId: cameraGroups)
    {
        nx::Uuid serviceId;
        if (!channelsByService.empty())
        {
            for (; it != channelsByService.end() && it->second == 0; ++it);
            if (it != channelsByService.end())
            {
                serviceId = it->first;
                --it->second;
            }
            else
            {
                serviceId = defaultServiceId;
            }
        }
        result[serviceId].insert(cameraGroupId);
    }
    return result;
}

LiveViewUsageHelper::LiveViewUsageHelper(SystemContext* context, QObject* parent):
    CloudServiceUsageHelper(context, parent)
{
}

LicenseSummaryDataEx LiveViewUsageHelper::info() const
{
    LicenseSummaryDataEx result;

    const auto cameras = getAllCameras();

    if (systemContext()->saasServiceManager()->saasServiceOperational())
    {
        // Add local recording services as well.
        for (const auto& [serviceId, parameters]:
            systemContext()->saasServiceManager()->localRecording())
        {
            result.available += parameters.totalChannelNumber;
        }
        for (const auto& camera: cameras)
        {
            if (camera->isScheduleEnabled())
                --result.available;
        }
    }
    else
    {
        // Add old licenses
        nx::vms::license::CamLicenseUsageHelper helper(systemContext());
        for (Qn::LicenseType licenseType: helper.licenseTypes())
        {
            result.available +=
                helper.totalLicenses(licenseType) - helper.usedLicenses(licenseType);
        }
    }

    result.available = std::max(0, result.available);

    for (const auto& [serviceId, parameters]: systemContext()->saasServiceManager()->liveView())
        result.available += parameters.totalChannelNumber;

    for (const auto& camera: cameras)
    {
        if (!camera->hasFlags(Qn::desktop_camera) && !camera->isScheduleEnabled()
            && camera->isOnline() && camera->hasVideo())
        {
            ++result.inUse;
            if (result.inUse > result.available)
                result.exceedDevices.insert(camera->getId());
        }
    }
    return result;
}

} // nx::vms::license::saas
