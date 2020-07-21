#include "object_type_dictionary.h"

#include <nx/analytics/object_type_descriptor_manager.h>

namespace nx::vms::client::desktop::analytics {

ObjectTypeDictionary::ObjectTypeDictionary(nx::analytics::ObjectTypeDescriptorManager* manager):
    m_objectTypeManager(manager)
{
}

std::optional<QString> ObjectTypeDictionary::idToName(const QString& id) const
{
    if (const auto descriptor = m_objectTypeManager->descriptor(id))
        return descriptor->name;

    return std::nullopt;
}

} // namespace nx::vms::client::desktop::analytics
