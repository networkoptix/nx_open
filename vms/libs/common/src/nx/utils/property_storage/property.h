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
        Storage* storage, const QString& name, const QString& description = QString());

signals:
    void changed(BaseProperty* property);

protected:
    void notify();
    void save(const QString& value);
    virtual void updateValue(const QString& value) = 0;

public:
    Storage* const storage;
    const QString name;
    const QString description;

    friend class Storage;
};

template<typename T>
class Property: public BaseProperty
{
public:
    Property(
        Storage* storage,
        const QString& name,
        const T& defaultValue = {},
        const QString& description = QString())
        :
        BaseProperty(storage, name, description),
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

        save(encodeValue(m_value));
    }

    Property& operator=(const T& value)
    {
        setValue(value);
        return *this;
    }

protected:
    virtual T decodeValue(const QString& value, bool* success) const
    {
        return QJson::deserialized<T>(value.toUtf8(), {}, success);
    }

    virtual QString encodeValue(const T& value) const
    {
        return QString::fromUtf8(QJson::serialized(value));
    }

    virtual void updateValue(const QString& value) override
    {
        bool ok = false;
        auto newValue = decodeValue(value, &ok);
        if (!ok || m_value == newValue)
            return;

        m_value = newValue;
        notify();
    }

private:
    T m_value;

    friend class Storage;
};

} // namespace nx::utils::property_storage
