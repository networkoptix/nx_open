#include "descriptor_list_manager.h"

#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics {

namespace details { //< TODO: Rename to "detail".

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

DescriptorListManager::DescriptorListManager(QnCommonModule* commonModule):
    base_type(commonModule)
{
    using namespace nx::vms::api::analytics;
    registerType<PluginDescriptor>("analyticsPluginDescriptorList");
    registerType<GroupDescriptor>("analyticsGroupDescriptorList");
    registerType<EventTypeDescriptor>("analyticsEventTypeDescriptorList");
    registerType<ObjectTypeDescriptor>("analyticsObjectTypeDescriptorList");
    registerType<ActionTypeDescriptor>("analyticsActionTypeDescriptor");
}

QString DescriptorListManager::propertyName(const std::type_info& typeInfo) const
{
    auto itr = m_typeMap.find(typeInfo);
    if (itr == m_typeMap.cend())
        return QString();

    return itr->second;
}

} // namespace nx::analytics
