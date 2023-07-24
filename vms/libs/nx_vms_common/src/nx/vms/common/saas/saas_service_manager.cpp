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

bool ServiceManager::loadSaasData(const std::string_view& data)
{
    const auto result = nx::reflect::json::deserialize(data, &m_data);
    if (result)
    {
        auto propertyDict = systemContext()->resourcePropertyDictionary();
        if (propertyDict->setValue(QnUuid(), kSaasDataPropertyKey, QString::fromUtf8(data)))
        {
            propertyDict->saveParamsAsync(QnUuid());
            onDataChanged();
        }
    }
    return result;
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
        for (const auto& service: services)
            m_services.emplace(service.id, service);

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
    const auto license = localRecordingLicenseV1();
    if (m_data.state != SaasState::uninitialized)
        systemContext()->licensePool()->addLicense(license);
    else
        systemContext()->licensePool()->removeLicense(license);
}

QnLicensePtr ServiceManager::localRecordingLicenseV1()  const
{
    using namespace nx::vms::api;

    QnLicensePtr license = QnLicense::createSaasLocalRecordingLicense();
    if (m_data.state != SaasState::active)
        return license;

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
    return m_data;
}

std::map<QnUuid, nx::vms::api::SaasService> ServiceManager::services() const
{
    return m_services;
}

} // nx::vms::common::saas
