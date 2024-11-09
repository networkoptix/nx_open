// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_service_manager.h"

#include <optional>

#include <api/runtime_info_manager.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/videowall_resource.h>
#include <licensing/license.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/reflect/json/serializer.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/ldap.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_resource_manager.h>
#include <nx/utils/qt_direct_connect.h>
#include <utils/common/synctime.h>

static const QString kAdditionTierLimitsPropertyKey("additionTierLimits");
static const QString kSaasDataPropertyKey("saasData");
static const QString kSaasServicesPropertyKey("saasServices");

using namespace nx::vms::api;

namespace {

constexpr std::array<SaasTierLimitName, 5> checkedTiers =
{
    SaasTierLimitName::maxServers,
    SaasTierLimitName::maxDevicesPerServer,
    SaasTierLimitName::maxDevicesPerLayout,
    SaasTierLimitName::ldapEnabled,
    SaasTierLimitName::videowallEnabled
};

void setJsonToDictionaryAsync(
    QnResourcePropertyDictionary* dictionary,
    const QString& propertyKey,
    const std::string_view& json)
{
    if (dictionary->setValue(nx::Uuid(), propertyKey, QString::fromUtf8(json)))
        dictionary->saveParamsAsync(nx::Uuid());
}

void setJsonToDictionarySync(QnResourcePropertyDictionary* dictionary,
    const QString& propertyKey,
    const std::string_view& json)
{
    if (dictionary->setValue(nx::Uuid(), propertyKey, QString::fromUtf8(json)))
        dictionary->saveParams(nx::Uuid());
}

template <typename T>
std::optional<T> getObjectFromDictionary(
    QnResourcePropertyDictionary* dictionary,
    const QString& propertyKey)
{
    const auto array = dictionary->value(nx::Uuid(), propertyKey).toUtf8();
    T result;
    if (array.isEmpty())
        return result;

    return nx::reflect::json::deserialize(std::string_view(array.data(), array.size()), &result)
        ? result
        : std::optional<T>();
}

} // namespace

namespace nx::vms::common::saas {

ServiceManager::ServiceManager(SystemContext* context, QObject* parent):
    QObject(parent),
    SystemContextAware(context),
    m_tierGracePeriodExpirationTime([this]() { return calculateGracePeriodTime(); })
{
    connect(
        resourcePropertyDictionary(), &QnResourcePropertyDictionary::propertyChanged,
        [this](const nx::Uuid& resourceId, const QString& key)
        {
            if (!resourceId.isNull())
                return;

            const auto dictionary = resourcePropertyDictionary();
            if (key == kAdditionTierLimitsPropertyKey || key == kSaasDataPropertyKey)
            {
                auto saasData = getObjectFromDictionary<SaasData>(
                    dictionary, kSaasDataPropertyKey);
                if (saasData)
                    setData(saasData.value_or(SaasData()), localTierLimits());
            }
            else if (key == kSaasServicesPropertyKey)
            {
                if (const auto services =
                    getObjectFromDictionary<std::vector<api::SaasService>>(dictionary, key))
                {
                    setServices(services.value());
                }
                else
                {
                    NX_ASSERT(false,
                        "Could not deserialize list of nx::vms::api::SaasService structures");
                }
            }
        });

    connect(
        resourcePropertyDictionary(), &QnResourcePropertyDictionary::propertyRemoved,
        [this](const nx::Uuid& resourceId, const QString& key)
        {
            if (!resourceId.isNull())
                return;

            if (key == kSaasDataPropertyKey)
                setData({}, {});
            else if (key == kSaasServicesPropertyKey)
                setServices({});
        });

    m_qtSignalGuards.push_back(nx::qtDirectConnect(
        runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoAdded,
        [this](const QnPeerRuntimeInfo&)
        {
            resetGracePeriodCache();
        }));
    m_qtSignalGuards.push_back(nx::qtDirectConnect(
        runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoChanged,
        [this](const QnPeerRuntimeInfo&)
        {
            resetGracePeriodCache();
        }));
    m_qtSignalGuards.push_back(nx::qtDirectConnect(
        runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoRemoved,
        [this](const QnPeerRuntimeInfo&)
        {
            resetGracePeriodCache();
        }));
}

void ServiceManager::resetGracePeriodCache()
{
    m_tierGracePeriodExpirationTime.reset();
}

void ServiceManager::loadSaasData(const std::string_view& saasDataJson)
{
    setJsonToDictionaryAsync(
        resourcePropertyDictionary(),
        kSaasDataPropertyKey,
        saasDataJson);
}

QByteArray ServiceManager::rawData() const
{
    return systemContext()->resourcePropertyDictionary()->value(
        nx::Uuid(), kSaasDataPropertyKey).toUtf8();
}

void ServiceManager::loadServiceData(const std::string_view& servicesJson)
{
    setJsonToDictionaryAsync(
        resourcePropertyDictionary(),
        kSaasServicesPropertyKey,
        servicesJson);
}

SaasData ServiceManager::data() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_data;
}

std::map<nx::Uuid, SaasService> ServiceManager::services() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_services;
}

api::SaasState ServiceManager::saasState() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_data.state;
}

void ServiceManager::setSaasStateAsync(api::SaasState saasState)
{
    setSaasStateInternal(saasState, /*waitForDone*/ false);
}

void ServiceManager::setSaasStateSync(api::SaasState saasState)
{
    setSaasStateInternal(saasState, /*waitForDone*/ true);
}

void ServiceManager::setSaasStateInternal(api::SaasState saasState, bool waitForDone)
{
    if (ec2::AbstractECConnectionPtr connection = systemContext()->messageBusConnection())
    {
        auto d = this->data();
        d.state = saasState;

        std::shared_ptr<std::promise<void>> promise;
        if (waitForDone)
            promise = std::make_unique<std::promise<void>>();

        ResourceParamWithRefData data(
            nx::Uuid(),
            kSaasDataPropertyKey,
            QString::fromStdString(nx::reflect::json::serialize(d)));
        ResourceParamWithRefDataList dataList;
        dataList.push_back(data);
        connection->getResourceManager(nx::network::rest::kSystemSession)->save(
            dataList,
            [promise](int, ec2::ErrorCode)
            {
                if (promise)
                    promise->set_value();
            });
        if (promise)
            promise->get_future().wait();
    }
}

std::map<nx::Uuid, SaasAnalyticsParameters> ServiceManager::analyticsIntegrations() const
{
    return purchasedServices<SaasAnalyticsParameters>(SaasService::kAnalyticsIntegrationServiceType);
}

std::map<nx::Uuid, SaasLocalRecordingParameters> ServiceManager::localRecording() const
{
    return purchasedServices<SaasLocalRecordingParameters>(SaasService::kLocalRecordingServiceType);
}

std::map<nx::Uuid, SaasCloudStorageParameters> ServiceManager::cloudStorageData() const
{
    return purchasedServices<SaasCloudStorageParameters>(SaasService::kCloudRecordingType);
}

bool ServiceManager::saasActive() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_data.state == SaasState::active;
}

bool ServiceManager::saasSuspended() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_data.state == SaasState::suspended;
}

bool ServiceManager::saasSuspendedOrShutDown() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return saasSuspendedOrShutDown(m_data.state);
}

bool ServiceManager::saasSuspendedOrShutDown(SaasState state)
{
    return state == SaasState::suspended || saasShutDown(state);
}

bool ServiceManager::saasShutDown() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return saasShutDown(m_data.state);
}

bool ServiceManager::saasShutDown(SaasState state)
{
    return state == SaasState::shutdown || state == SaasState::autoShutdown;
}

void ServiceManager::updateLocalRecordingLicenseV1()
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    updateLocalRecordingLicenseV1Unsafe();
}

ServiceTypeStatus ServiceManager::serviceStatus(const QString& serviceType) const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_data.security.status.contains(serviceType)
        ? m_data.security.status.at(serviceType)
        : ServiceTypeStatus();
}

void ServiceManager::setData(
    SaasData data,
    LocalTierLimits localTierLimits)
{
    for (const auto& [tierName, value]: localTierLimits)
    {
        if (!data.tierLimits.contains(tierName))
        {
            if (value.has_value())
                data.tierLimits[tierName] = *value;
            else
                data.tierLimits.erase(tierName);
        }
    }

    bool stateChanged = false;
    {
        NX_MUTEX_LOCKER mutexLocker(&m_mutex);

        if (m_data == data)
            return;

        stateChanged = m_data.state != data.state;

        m_data = data;
        updateLocalRecordingLicenseV1Unsafe();
    }

    emit dataChanged();
    if (stateChanged)
        emit saasStateChanged();
}

void ServiceManager::setServices(const std::vector<SaasService>& services)
{
    {
        NX_MUTEX_LOCKER mutexLocker(&m_mutex);
        std::map<nx::Uuid, SaasService> servicesMap;
        for (const auto& service: services)
            servicesMap.emplace(service.id, service);

        if (m_services == servicesMap)
            return;

        m_services = servicesMap;
        updateLocalRecordingLicenseV1Unsafe();
    }

    emit dataChanged();
}

void ServiceManager::setEnabled(bool value)
{
    m_enabled = value;
}

bool ServiceManager::isEnabled() const
{
    return m_enabled;
}

QnLicensePtr ServiceManager::localRecordingLicenseV1Unsafe() const
{
    QnLicensePtr license = QnLicense::createSaasLocalRecordingLicense();
    if (m_data.state == SaasState::uninitialized || saasShutDown(m_data.state))
    {
        // Dont use SAAS -> licenseV1 conversion for shutdown state.
        // Local recording will be stopped due there are no V1 licenses.
        return license;
    }

    int counter = 0;
    for (const auto& [serviceId, purshase]: m_data.services)
    {
        if (auto it = m_services.find(serviceId); it != m_services.end())
        {
            const SaasService& service = it->second;
            if (service.type != SaasService::kLocalRecordingServiceType)
                continue;

            const auto params = SaasLocalRecordingParameters::fromParams(service.parameters);
            counter += params.totalChannelNumber * purshase.quantity;
        }
    }
    license->setCameraCount(counter);
    license->setTmpExpirationDate(m_data.security.tmpExpirationDate);

    return license;
}

void ServiceManager::updateLocalRecordingLicenseV1Unsafe()
{
    const auto license = localRecordingLicenseV1Unsafe();
    if (m_data.state != SaasState::uninitialized)
        systemContext()->licensePool()->addLicense(license);
    else
        systemContext()->licensePool()->removeLicense(license);
}

template <typename ServiceParamsType>
std::map<nx::Uuid, ServiceParamsType> ServiceManager::purchasedServices(
    const QString& serviceType) const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);

    const auto& purchases = m_data.services;
    std::map<nx::Uuid, ServiceParamsType> result;
    for (const auto& [serviceId, service]: m_services)
    {
        if (service.type != serviceType)
            continue;

        ServiceParamsType params = ServiceParamsType::fromParams(service.parameters);
        int quantity = 0;
        if (auto purshaseIt = purchases.find(serviceId); purshaseIt != purchases.end())
            quantity = purshaseIt->second.quantity;
        params.totalChannelNumber *= quantity;

        const auto itResult = result.find(serviceId);
        if (itResult == result.end())
            result.emplace(serviceId, params);
        else
            itResult->second.totalChannelNumber += params.totalChannelNumber;
    }

    return result;
}

std::optional<int> ServiceManager::tierLimit(SaasTierLimitName value) const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return tierLimitUnsafe(value);
}

std::optional<int> ServiceManager::tierLimitUnsafe(SaasTierLimitName value) const
{
    auto it = m_data.tierLimits.find(value);
    if (it != m_data.tierLimits.end())
        return it->second;
    return std::nullopt;
}

std::optional<int> ServiceManager::camerasTierLimitLeft(const nx::Uuid& serverId)
{
    const std::optional<int> limit = tierLimit(SaasTierLimitName::maxDevicesPerServer);
    if (!limit.has_value())
        return std::nullopt; // there is no limit

    const int cameraCount = resourcePool()->getAllCameras(
        serverId, /*ignoreDesktopCameras*/ true).size();
    return *limit - cameraCount;
}

int ServiceManager::tierLimitUsed(SaasTierLimitName value) const
{
    switch (value)
    {
        case SaasTierLimitName::maxDevicesPerServer:
        {
            int result = 0;
            for (const auto& server: resourcePool()->servers())
            {
                int cameraCount = resourcePool()->getAllCameras(
                    server, /*ignoreDesktopCameras*/ true).size();
                result = std::max(result, cameraCount);
            }
            return result;
        }
        case SaasTierLimitName::maxServers:
        {
            return resourcePool()->servers().size();
        }

        case SaasTierLimitName::maxDevicesPerLayout:
        {
            const auto layouts = resourcePool()->getResources<QnLayoutResource>();
            int existing = 0;
            for (const auto& layout: layouts)
                existing = std::max(existing, (int) layout->getItems().size());
            return existing;
        }

        case SaasTierLimitName::videowallEnabled:
            return resourcePool()->getResources<QnVideoWallResource>().isEmpty() ? 0 : 1;

        case SaasTierLimitName::ldapEnabled:
            return globalSettings()->ldap().isValid(/*checkPassword*/ false) ? 1 : 0;

    }
    NX_ASSERT(0, "Not implemented %1", static_cast<int>(value));
    return 0;
}

std::optional<int> ServiceManager::tierLimitLeft(SaasTierLimitName value) const
{
    const std::optional<int> limit = tierLimit(value);
    if (!limit.has_value())
        return std::nullopt; // There is no limit.
    return *limit - tierLimitUsed(value);
}

bool ServiceManager::tierLimitReached(SaasTierLimitName value) const
{
    const auto limit = tierLimit(value);
    return limit && tierLimitUsed(value) > *limit;
}

bool ServiceManager::hasFeature(SaasTierLimitName value) const
{
    const auto limit = tierLimitLeft(value);
    return limit.value_or(1) > 0; //< Features are enabled by default.
}

void ServiceManager::setLocalTierLimits(
    const ServiceManager::LocalTierLimits& tierLimits)
{
    setJsonToDictionarySync(resourcePropertyDictionary(),
        kAdditionTierLimitsPropertyKey,
        nx::reflect::json::serialize(tierLimits));
}

ServiceManager::LocalTierLimits ServiceManager::localTierLimits() const
{
    return getObjectFromDictionary<LocalTierLimits>(resourcePropertyDictionary(),
        kAdditionTierLimitsPropertyKey).value_or(LocalTierLimits());
}

std::chrono::milliseconds ServiceManager::tierGracePeriodExpirationTime() const
{
    return m_tierGracePeriodExpirationTime.get();
}

std::chrono::milliseconds ServiceManager::calculateGracePeriodTime() const
{
    std::optional<qint64> resultMs;
    systemContext()->runtimeInfoManager()->hasItem(
        [&resultMs](const auto& item)
        {
            if (item.data.tierGracePeriodExpirationDateMs.count() == 0)
                return false;
            if (resultMs.has_value())
            {
                resultMs = std::min(*resultMs,
                    (qint64) item.data.tierGracePeriodExpirationDateMs.count());
            }
            else
            {
                resultMs = item.data.tierGracePeriodExpirationDateMs.count();
            }
            return false;
        });
    return std::chrono::milliseconds(resultMs.value_or(0));
}

bool ServiceManager::isTierGracePeriodStarted() const
{
    return tierGracePeriodExpirationTime().count() > 0;
}

bool ServiceManager::isTierGracePeriodExpired() const
{
    const qint64 expiredMs = tierGracePeriodExpirationTime().count();
    return expiredMs > 0 && qnSyncTime->currentMSecsSinceEpoch() > expiredMs;
}

std::optional<int> ServiceManager::tierGracePeriodDaysLeft() const
{
    const qint64 expiredMs = tierGracePeriodExpirationTime().count();
    if (expiredMs == 0)
        return std::nullopt;

    const QDate expiredDate = QDateTime::fromMSecsSinceEpoch(expiredMs).date();
    const QDate currentDate = qnSyncTime->currentDateTime().date();
    return (int) std::max(0LL, expiredDate.toJulianDay() - currentDate.toJulianDay());
}

bool ServiceManager::hasTierOveruse() const
{
    const auto saas = systemContext()->saasServiceManager();
    return std::any_of(checkedTiers.begin(),
        checkedTiers.end(),
        [saas](const auto limit) { return saas->tierLimitReached(limit); });
}

std::map<nx::Uuid, int> ServiceManager::overusedResourcesUnsafe(SaasTierLimitName feature) const
{
    std::map<nx::Uuid, int> result;
    const std::optional<int> limit = tierLimitUnsafe(feature);
    if (!limit.has_value())
        return result;

    switch (feature)
    {
        case SaasTierLimitName::maxDevicesPerServer:
        {
            for (const auto& server: systemContext()->resourcePool()->servers())
            {
                auto cameras = resourcePool()->getAllCameras(
                    server, /*ignoreDesktopCameras*/ true);
                if (cameras.size() > *limit)
                    result.emplace(server->getId(), cameras.size());
            }
            break;
        }
        case SaasTierLimitName::maxDevicesPerLayout:
        {
            const auto layouts = resourcePool()->getResources<QnLayoutResource>();
            for (const auto& layout: layouts)
            {
                if (layout->getItems().size() > *limit)
                    result.emplace(layout->getId(), layout->getItems().size());
            }
            break;
        }
        default:
            break;
    }
    return result;
}

TierOveruseMap ServiceManager::tierOveruseDetails() const
{
    TierOveruseMap result;
    NX_MUTEX_LOCKER lock(&m_mutex);
    for (const auto& feature: checkedTiers)
    {
        const std::optional<int> limit = tierLimitUnsafe(feature);
        if (!limit.has_value())
            continue;

        TierOveruseData data;
        data.used = tierLimitUsed(feature);
        if (data.used > *limit)
        {
            data.allowed = *limit;
            data.details = overusedResourcesUnsafe(feature);
            result.emplace(feature, std::move(data));
        }
    }
    return result;
}

TierUsageMap ServiceManager::tiersUsageDetails() const
{
    TierUsageMap result;

    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    for (const auto& feature: checkedTiers)
    {
        if (auto limit = tierLimitUnsafe(feature))
        {
            TierUsageData data;
            data.allowed = *limit;
            data.used = tierLimitUsed(feature);
            result.emplace(feature, std::move(data));
        }
    }
    return result;
}

} // nx::vms::common::saas
