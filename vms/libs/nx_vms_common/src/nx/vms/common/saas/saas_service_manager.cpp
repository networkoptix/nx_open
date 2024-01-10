// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_service_manager.h"

#include <optional>

#include <core/resource_management/resource_properties.h>
#include <licensing/license.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_resource_manager.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/reflect/json/serializer.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/log/log.h>
#include <nx/vms/common/system_context.h>

static const QString kSaasDataPropertyKey("saasData");
static const QString kSaasServicesPropertyKey("saasServices");

namespace {

void setJsonToDictionary(
    QnResourcePropertyDictionary* dictionary,
    const QString& propertyKey,
    const std::string_view& json)
{
    if (dictionary->setValue(QnUuid(), propertyKey, QString::fromUtf8(json)))
        dictionary->saveParamsAsync(QnUuid());
}

template <typename T>
std::optional<T> getObjectFromDictionary(
    QnResourcePropertyDictionary* dictionary,
    const QString& propertyKey)
{
    const auto array = dictionary->value(QnUuid(), propertyKey).toUtf8();
    T result;
    return nx::reflect::json::deserialize(std::string_view(array.data(), array.size()), &result)
        ? result
        : std::optional<T>();
}

} // namespace

namespace nx::vms::common::saas {

ServiceManager::ServiceManager(SystemContext* context, QObject* parent):
    QObject(parent),
    SystemContextAware(context)
{
    using namespace nx::vms::api;

    connect(resourcePropertyDictionary(), &QnResourcePropertyDictionary::propertyChanged,
        [this](const QnUuid& resourceId, const QString& key)
        {
            if (!resourceId.isNull())
                return;

            const auto dictionary = resourcePropertyDictionary();
            if (key == kSaasDataPropertyKey)
            {
                if (const auto saasData = getObjectFromDictionary<api::SaasData>(dictionary, key))
                    setData(saasData.value());
                else
                    NX_ASSERT(false, "Could not deserialize nx::vms::api::SaasData structure");
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

    connect(resourcePropertyDictionary(), &QnResourcePropertyDictionary::propertyRemoved,
        [this](const QnUuid& resourceId, const QString& key)
        {
            if (!resourceId.isNull())
                return;

            if (key == kSaasDataPropertyKey)
                setData({});
            else if (key == kSaasServicesPropertyKey)
                setServices({});
        });
}

void ServiceManager::loadSaasData(const std::string_view& saasDataJson)
{
    setJsonToDictionary(
        resourcePropertyDictionary(),
        kSaasDataPropertyKey,
        saasDataJson);
}

void ServiceManager::loadServiceData(const std::string_view& servicesJson)
{
    setJsonToDictionary(
        resourcePropertyDictionary(),
        kSaasServicesPropertyKey,
        servicesJson);
}

nx::vms::api::SaasData ServiceManager::data() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_data;
}

std::map<QnUuid, nx::vms::api::SaasService> ServiceManager::services() const
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

        nx::vms::api::ResourceParamWithRefData data(
            QnUuid(),
            kSaasDataPropertyKey,
            QString::fromStdString(nx::reflect::json::serialize(d)));
        nx::vms::api::ResourceParamWithRefDataList dataList;
        dataList.push_back(data);
        connection->getResourceManager(Qn::kSystemAccess)->save(
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

std::map<QnUuid, nx::vms::api::SaasAnalyticsParameters> ServiceManager::analyticsIntegrations() const
{
    using namespace nx::vms::api;
    return purchasedServices<SaasAnalyticsParameters>(SaasService::kAnalyticsIntegrationServiceType);
}

std::map<QnUuid, nx::vms::api::SaasLocalRecordingParameters> ServiceManager::localRecording() const
{
    using namespace nx::vms::api;
    return purchasedServices<SaasLocalRecordingParameters>(SaasService::kLocalRecordingServiceType);
}

std::map<QnUuid, nx::vms::api::SaasCloudStorageParameters> ServiceManager::cloudStorageData() const
{
    using namespace nx::vms::api;
    return purchasedServices<SaasCloudStorageParameters>(SaasService::kCloudRecordingType);
}

bool ServiceManager::saasActive() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_data.state == nx::vms::api::SaasState::active;
}

bool ServiceManager::saasSuspended() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_data.state == nx::vms::api::SaasState::suspend;
}

bool ServiceManager::saasSuspendedOrShutDown() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return saasSuspendedOrShutDown(m_data.state);
}

bool ServiceManager::saasSuspendedOrShutDown(nx::vms::api::SaasState state)
{
    using namespace nx::vms::api;
    return state == SaasState::suspend || saasShutDown(state);
}

bool ServiceManager::saasShutDown() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return saasShutDown(m_data.state);
}

bool ServiceManager::saasShutDown(nx::vms::api::SaasState state)
{
    using namespace nx::vms::api;
    return state == SaasState::shutdown || state == SaasState::autoShutdown;
}

void ServiceManager::updateLocalRecordingLicenseV1()
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    updateLocalRecordingLicenseV1Unsafe();
}

nx::vms::api::ServiceTypeStatus ServiceManager::serviceStatus(const QString& serviceType) const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_data.security.status.contains(serviceType)
        ? m_data.security.status.at(serviceType)
        : nx::vms::api::ServiceTypeStatus();
}

void ServiceManager::setData(const nx::vms::api::SaasData& data)
{
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

void ServiceManager::setServices(const std::vector<nx::vms::api::SaasService>& services)
{
    {
        NX_MUTEX_LOCKER mutexLocker(&m_mutex);
        std::map<QnUuid, nx::vms::api::SaasService> servicesMap;
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
    using namespace nx::vms::api;

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
    using namespace nx::vms::api;

    const auto license = localRecordingLicenseV1Unsafe();
    if (m_data.state != SaasState::uninitialized)
        systemContext()->licensePool()->addLicense(license);
    else
        systemContext()->licensePool()->removeLicense(license);
}

template <typename ServiceParamsType>
std::map<QnUuid, ServiceParamsType> ServiceManager::purchasedServices(
    const QString& serviceType) const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);

    std::map<QnUuid, ServiceParamsType> result;

    for (const auto& [serviceId, purshase]: m_data.services)
    {
        if (auto it = m_services.find(serviceId); it != m_services.end())
        {
            const auto& service = it->second;
            if (service.type != serviceType)
                continue;

            ServiceParamsType params = ServiceParamsType::fromParams(service.parameters);
            params.totalChannelNumber *= purshase.quantity;
            const auto itResult = result.find(serviceId);
            if (itResult == result.end())
                result.emplace(serviceId, params);
            else
                itResult->second.totalChannelNumber += params.totalChannelNumber;
        }
    }

    return result;
}

} // nx::vms::common::saas
