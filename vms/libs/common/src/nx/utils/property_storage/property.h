#pragma once

#include <QtCore/QObject>

#include <nx/fusion/serialization/json_fwd.h>

namespace nx::utils::property_storage {

class Storage;

class BaseProperty: public QObject
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

protected:
    virtual T deserializeValue(const QString& value, bool* success) const
    {
        return QJson::deserialized<T>(value.toUtf8(), {}, success);
    }

    virtual QString serializeValue(const T& value) const
    {
        return QString::fromUtf8(QJson::serialized(value));
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
