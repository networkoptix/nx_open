#include "descriptor_list_manager.h"

#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics {

namespace details {

template<>
std::set<QString> descriptorIds<nx::vms::api::analytics::EventTypeDescriptor>(
    const QnVirtualCameraResourcePtr& device)
{
    std::set<QString> result;
    auto supportedEventTypes = device->supportedAnalyticsEventTypeIds();
    for (const auto& eventTypeIdSet: supportedEventTypes)
        result.insert(eventTypeIdSet.cbegin(), eventTypeIdSet.cend());

    return result;
}

template<>
std::set<QString> descriptorIds<nx::vms::api::analytics::ObjectTypeDescriptor>(
    const QnVirtualCameraResourcePtr& device)
{
    std::set<QString> result;
    auto supportedObjectTypes = device->supportedAnalyticsObjectTypeIds();
    for (const auto& objectTypeIdSet: supportedObjectTypes)
        result.insert(objectTypeIdSet.cbegin(), objectTypeIdSet.cend());

    return result;
}

} // namespace details

namespace analytics_api = nx::vms::api::analytics;

DescriptorListManager::DescriptorListManager(QnCommonModule* commonModule):
    base_type(commonModule)
{
    registerType<analytics_api::PluginDescriptor>("analyticsPluginDescriptorList");
    registerType<analytics_api::GroupDescriptor>("analyticsGroupDescriptorList");
    registerType<analytics_api::EventTypeDescriptor>("analyticsEventTypeDescriptorList");
    registerType<analytics_api::ObjectTypeDescriptor>("analyticsObjectTypeDescriptorList");
    registerType<analytics_api::ActionTypeDescriptor>("analyticsActionTypeDescriptor");
}

QString DescriptorListManager::propertyName(const std::type_info& typeInfo) const
{
    auto itr = m_typeMap.find(typeInfo);
    if (itr == m_typeMap.cend())
        return QString();

    return itr->second;
}

} // namespace nx::analytics


