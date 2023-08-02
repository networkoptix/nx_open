// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_service_manager.h"

#include <core/resource_management/resource_properties.h>
#include <licensing/license.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/reflect/json/serializer.h>
#include <nx/reflect/string_conversion.h>
#include <nx/vms/common/system_context.h>
#include <nx/utils/log/log.h>

static const QString kSaasDataPropertyKey("saasData");
static const QString kSaasServicesPropertyKey("saasServices");

namespace nx::vms::common::saas {

ServiceManager::ServiceManager(SystemContext* context, QObject* parent):
    QObject(parent),
    SystemContextAware(context)
{
        connect(
            context->resourcePropertyDictionary(),
            &QnResourcePropertyDictionary::propertyChanged,
            [this](const QnUuid& resourceId, const QString& key)
            {
                if (!resourceId.isNull())
                    return;

                if (key == kSaasDataPropertyKey)
                {
                    const auto dict = systemContext()->resourcePropertyDictionary();
                    const auto value = dict->value(QnUuid(), kSaasDataPropertyKey).toUtf8();
                    loadSaasData(std::string_view(value.data(), value.size()));
                }
                else if (key == kSaasServicesPropertyKey)
                {
                    const auto dict = systemContext()->resourcePropertyDictionary();
                    const auto value = dict->value(QnUuid(), kSaasServicesPropertyKey).toUtf8();
                    loadServiceData(std::string_view(value.data(), value.size()));
                }
            });
}

void ServiceManager::setData(const nx::vms::api::SaasData& data)
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    m_data = data;
}

void ServiceManager::loadSaasData(const nx::vms::api::SaasData& data)
{
    setData(data);
    saveSaasDataToProperty(nx::reflect::json::serialize(data));
}

void ServiceManager::saveSaasDataToProperty(const std::string_view& data)
{
    auto propertyDict = systemContext()->resourcePropertyDictionary();
    if (propertyDict->setValue(QnUuid(), kSaasDataPropertyKey, QString::fromUtf8(data)))
    {
        propertyDict->saveParamsAsync(QnUuid());
        onDataChanged();
    }
}

bool ServiceManager::loadSaasData(const std::string_view& data)
{
    nx::vms::api::SaasData value;
    if (!nx::reflect::json::deserialize(data, &value))
        return false;
    setData(value);
    saveSaasDataToProperty(data);
    return true;
}

void ServiceManager::setDisabled()
{
    m_disabled = true;
}

bool ServiceManager::isEnabled() const
{
    return !m_disabled;
}

bool ServiceManager::loadServiceData(const std::string_view& data)
{
    std::vector<nx::vms::api::SaasService> services;
    bool result = nx::reflect::json::deserialize(data, &services);
    if (result)
    {
        {
            NX_MUTEX_LOCKER mutexLocker(&m_mutex);
            m_services.clear();
            for (const auto& service: services)
                m_services.emplace(service.id, service);
        }

        updateLicenseV1();

        auto propertyDict = systemContext()->resourcePropertyDictionary();
        if (propertyDict->setValue(QnUuid(), kSaasServicesPropertyKey, QString::fromUtf8(data)))
        {
            propertyDict->saveParamsAsync(QnUuid());
            onDataChanged();
        }
    }
    return result;
}

void ServiceManager::onDataChanged()
{
    updateLicenseV1();
    dataChanged();
}

void ServiceManager::updateLicenseV1()
{
    using namespace nx::vms::api;
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);

    const auto license = localRecordingLicenseV1Unsafe();
    if (m_data.state != SaasState::uninitialized)
        systemContext()->licensePool()->addLicense(license);
    else
        systemContext()->licensePool()->removeLicense(license);
}

QnLicensePtr ServiceManager::localRecordingLicenseV1() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return localRecordingLicenseV1Unsafe();
}

QnLicensePtr ServiceManager::localRecordingLicenseV1Unsafe() const
{
    using namespace nx::vms::api;

    QnLicensePtr license = QnLicense::createSaasLocalRecordingLicense();
    if (m_data.state == SaasState::uninitialized || m_data.state == SaasState::shutdown)
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

template <typename ServiceParamsType>
std::map<QnUuid, ServiceParamsType> ServiceManager::purchasedServices(const QString& serviceType) const
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

nx::vms::api::SaasData ServiceManager::data() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_data;
}

std::pair<bool, QString> ServiceManager::isBlocked() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    using namespace nx::vms::api;
    const auto state = m_data.state;
    if (state == SaasState::suspend || state == SaasState::shutdown)
    {
        const QString message = NX_FMT(
            "Access for cloud users is forbiden because of System "
            "is in '%1' state", toString(state));
        return {true, message};
    }

    return {false, QString()};
}

std::map<QnUuid, nx::vms::api::SaasService> ServiceManager::services() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_services;
}

nx::vms::api::SaasState ServiceManager::state() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_data.state;
}

void ServiceManager::setState(nx::vms::api::SaasState state)
{
    auto data = this->data();
    data.state = state;
    loadSaasData(data);
}

nx::vms::api::ServiceTypeStatus ServiceManager::serviceState(const QString& serviceName) const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);

    using namespace nx::vms::api;
    const auto it = m_data.security.status.find(serviceName);
    return it == m_data.security.status.end() ? ServiceTypeStatus() : it->second;
}

} // nx::vms::common::saas
