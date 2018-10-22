#include "descriptor_list_manager.h"

#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics {

namespace analytics_api = nx::vms::api::analytics;

DescriptorListManager::DescriptorListManager(QnCommonModule* commonModule):
    base_type(commonModule)
{
    registerType<analytics_api::PluginDescriptor>("analyticsPluginDescriptorList");
    registerType<analytics_api::GroupDescriptor>("analyticsGroupDescriptorList");
    registerType<analytics_api::EventTypeDescriptor>("analyticsEventTypeDescriptorList");
    registerType<analytics_api::ObjectTypeDescriptor>("analyticsObjectTypeDescriptorList");
}

QString DescriptorListManager::propertyName(const std::type_info& typeInfo) const
{
    auto itr = m_typeMap.find(typeInfo);
    if (itr == m_typeMap.cend())
        return QString();

    return itr->second;
}

} // namespace nx::analytics


