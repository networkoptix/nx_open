// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>

#include "abstract_backend.h"
#include "property.h"

namespace nx::utils::property_storage {

class NX_VMS_COMMON_API Storage: public QObject
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

    bool isWritable() const;
    void registerProperty(BaseProperty* property);
    void unregisterProperty(BaseProperty* property);
    Q_INVOKABLE bool exists(const QString& name) const;
    Q_INVOKABLE QVariant value(const QString& name) const;
    Q_INVOKABLE void setValue(const QString& name, const QVariant& value);

signals:
    void changed(BaseProperty* property);

protected:
    void load();
    void sync();

    QByteArray securityKey() const;
    void setSecurityKey(const QByteArray& value);

    AbstractBackend* backend() const;

    QHash<QString, BaseProperty*> properties() const;

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
