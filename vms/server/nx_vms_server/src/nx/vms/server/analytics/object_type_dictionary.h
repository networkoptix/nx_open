#pragma once

#include <nx/analytics/db/abstract_object_type_dictionary.h>
#include <nx/analytics/object_type_descriptor_manager.h>

namespace nx::vms::server::analytics {

class ObjectTypeDictionary:
    public QObject,
    public nx::analytics::db::AbstractObjectTypeDictionary
{
    Q_OBJECT
public:
    ObjectTypeDictionary(nx::analytics::ObjectTypeDescriptorManager* manager);

    virtual std::optional<QString> idToName(const QString& id) const override;
private:
    const nx::analytics::ObjectTypeDescriptorManager* m_objectTypeManager = nullptr;
    
    mutable std::map<QString, QString> m_idToNameCache;
    mutable nx::Mutex m_mutex;
};

} // namespace nx::vms::server::analytics
