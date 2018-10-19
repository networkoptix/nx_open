#include "descriptor_list_manager.h"

namespace nx::mediaserver::analytics {

DescriptorListManager::DescriptorListManager(QnMediaServerModule* serverModule):
    base_type(serverModule)
{
}

QString DescriptorListManager::propertyName(const std::type_info& typeInfo) const
{
    auto itr = m_typeMap.find(typeInfo);
    if (itr == m_typeMap.cend())
        return QString();

    return itr->second;
}

} // namespace nx::mediaserver::analytics


