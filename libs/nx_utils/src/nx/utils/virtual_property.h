// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <concepts>

#include <QtCore/QObject>

namespace nx::utils {

class NX_UTILS_API VirtualPropertyBase: public QObject
{
    Q_OBJECT

public:
    VirtualPropertyBase(QObject* parent = nullptr):
        QObject(parent)
    {
    }

signals:
    void valueChanged();
};

/**
 * VirtualProperty is a value container which notifies subscribers when the value is changed.
 * This class is useful when you need a private QObject property and don't want to involve Qt moc
 * into the process.
 * Unlike QObject dynamic properties this class can hold any value, not requiring it to be
 * registered as a Qt metatype. Also it provides easily subscriptable signal while dynamic property
 * changes in QObject could be observed only with an event handler.
 */
template<typename T> requires(std::equality_comparable<T> && std::is_assignable_v<T, T>)
class VirtualProperty: public VirtualPropertyBase
{
public:
    VirtualProperty(const T& value = T{}, QObject* parent = nullptr):
        VirtualPropertyBase(parent),
        m_value(value)
    {
    }

    const T& value() const
    {
        return m_value;
    }

    void setValue(const T& value)
    {
        if (m_value == value)
            return;

        m_value = value;
        emit valueChanged();
    }

private:
    T m_value;
};

} // namespace nx::utils
