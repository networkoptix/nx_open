#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>

#include "property.h"
#include "abstract_backend.h"

namespace nx::utils::property_storage {

class Storage: public QObject
{
    Q_OBJECT

public:
    using BaseProperty = nx::utils::property_storage::BaseProperty;

    template<typename T>
    using Property = nx::utils::property_storage::Property<T>;

    template<typename T>
    using SecureProperty = nx::utils::property_storage::SecureProperty<T>;

    explicit Storage(AbstractBackend* backend, QObject* parent = nullptr);
    virtual ~Storage() = default;

    void registerProperty(BaseProperty* property);
    void unregisterProperty(BaseProperty* property);

protected:
    void load();
    void sync();

    void setSecurityKey(const QByteArray& value);

    QList<BaseProperty*> properties() const;

    void loadProperty(BaseProperty* property);
    void saveProperty(BaseProperty* property);

private:
    QString readValue(const QString& name);
    void writeValue(const QString& name, const QString& value);
    void removeValue(const QString& name);

private:
    QScopedPointer<AbstractBackend> m_backend;
    QHash<QString, BaseProperty*> m_properties;
    QByteArray m_securityKey;
};

} // namespace nx::utils::property_storage
