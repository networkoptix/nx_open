#pragma once

#include <QtCore/QHash>

#include "property.h"
#include "abstract_backend.h"

namespace nx::utils::property_storage {

class Storage
{
public:
    template<typename T>
    using Property = nx::utils::property_storage::Property<T>;
    using BaseProperty = nx::utils::property_storage::BaseProperty;

    explicit Storage(AbstractBackend* backend);
    virtual ~Storage() = default;

protected:
    void load();
    void sync();

    void registerProperty(BaseProperty* property);
    void unregisterProperty(BaseProperty* property);
    QList<BaseProperty*> properties() const;

    QString readValue(const QString& name);
    void writeValue(const QString& name, const QString& value);
    void removeValue(const QString& name);

private:
    QScopedPointer<AbstractBackend> m_backend;
    QHash<QString, BaseProperty*> m_properties;

    friend class nx::utils::property_storage::BaseProperty;
};

} // namespace nx::utils::property_storage
