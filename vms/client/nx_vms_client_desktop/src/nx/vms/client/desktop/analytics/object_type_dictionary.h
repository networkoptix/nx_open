#pragma once

#include <analytics/db/abstract_object_type_dictionary.h>

namespace nx::analytics { class ObjectTypeDescriptorManager; }

namespace nx::vms::client::desktop::analytics {

class ObjectTypeDictionary: public nx::analytics::db::AbstractObjectTypeDictionary
{
public:
    ObjectTypeDictionary(nx::analytics::ObjectTypeDescriptorManager* manager);

    virtual std::optional<QString> idToName(const QString& id) const override;

private:
    const nx::analytics::ObjectTypeDescriptorManager* m_objectTypeManager = nullptr;
};

} // namespace nx::vms::client::desktop::analytics
