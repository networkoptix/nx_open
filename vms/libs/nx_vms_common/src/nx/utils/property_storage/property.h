// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>

#include <nx/reflect/json.h>
#include <nx/utils/json/qt_core_types.h>

namespace nx::utils::property_storage {

class Storage;

class NX_VMS_COMMON_API BaseProperty: public QObject
{
    Q_OBJECT

public:
    BaseProperty(
        Storage* storage,
        const QString& name,
        const QString& description = QString(),
        bool secure = false);

    virtual QString serialized() const = 0;
    virtual void loadSerializedValue(const QString& value) = 0;
    virtual QVariant variantValue() const = 0;
    virtual void setVariantValue(const QVariant& value) = 0;
    bool exists() const;

signals:
    void changed(BaseProperty* property);

protected:
    void notify();

public:
    Storage* const storage;
    const QString name;
    const QString description;
    const bool secure;
};

template<typename T>
class Property: public BaseProperty
{
public:
    Property(
        Storage* storage,
        const QString& name,
        const T& defaultValue = {},
        const QString& description = QString(),
        bool secure = false)
        :
        BaseProperty(storage, name, description, secure),
        m_value(defaultValue)
    {
    }

    T value() const
    {
        return m_value;
    }

    T operator()() const
    {
        return m_value;
    }

    void setValue(const T& value)
    {
        if (m_value == value)
            return;

        m_value = value;
        notify();
    }

    Property& operator=(const T& value)
    {
        setValue(value);
        return *this;
    }

    virtual QString serialized() const override
    {
        return serializeValue(m_value);
    }

    virtual void loadSerializedValue(const QString& value) override
    {
        bool ok = false;
        auto newValue = deserializeValue(value, &ok);
        if (!ok || m_value == newValue)
            return;

        m_value = newValue;
        notify();
    }

    virtual QVariant variantValue() const override
    {
        return QVariant::fromValue(value());
    }

    virtual void setVariantValue(const QVariant& value) override
    {
        setValue(value.value<T>());
    }

protected:
    virtual T deserializeValue(const QString& value, bool* success) const
    {
        T result;
        auto status = nx::reflect::json::deserialize<T>(value.toStdString(), &result);
        if (success)
            *success = status.success;
        return result;
    }

    virtual QString serializeValue(const T& value) const
    {
        return QString::fromStdString(nx::reflect::json::serialize(value));
    }

private:
    T m_value;
};

template<typename T>
class SecureProperty: public Property<T>
{
    using base_type = Property<T>;

public:
    SecureProperty(
        Storage* storage,
        const QString& name,
        const T& defaultValue = {},
        const QString& description = QString())
        :
        base_type(storage, name, defaultValue, description, /*secure*/ true)
    {
    }

    using base_type::operator=;
};

} // namespace nx::utils::property_storage
