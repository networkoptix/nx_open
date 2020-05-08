#include "object_type_dictionary.h"

using namespace nx::vms::api;

namespace nx::vms::server::analytics {

ObjectTypeDictionary::ObjectTypeDictionary(nx::analytics::ObjectTypeDescriptorManager* manager):
    m_objectTypeManager(manager)
{
}

std::optional<QString> ObjectTypeDictionary::idToName(const QString& id) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (const auto itr = m_idToNameCache.find(id); itr != m_idToNameCache.end())
        return itr->second;

    if (const auto descriptor = m_objectTypeManager->descriptor(id))
    {
        m_idToNameCache[id] = descriptor->name;
        return descriptor->name;
    }
    return std::nullopt;
}

} // namespace nx::vms::server::analytics
