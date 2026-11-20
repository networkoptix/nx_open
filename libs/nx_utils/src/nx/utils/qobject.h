// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <type_traits>

#include <QtCore/QByteArrayView>
#include <QtCore/QList>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QObject>

#include <nx/utils/log/assert.h>

namespace nx::utils {

/** Describes access to the properties of the QObject based classes. */
enum class PropertyAccess
{
    /** Property must exists but not necessary must be readable or writable. */
    anyAccess = 0x0,

    /** Property must exists and must be readable at least. */
    readable = 0x1,

    /** Property must exists and must be writable at least. */
    writable = 0x2,

    /** Property must exists and must be readable and writable. */
    fullAccess = readable | writable,

    /** The property must exists and have stored flag. */
    stored = 0x4,
};
Q_DECLARE_FLAGS(PropertyAccessFlags, PropertyAccess)
Q_DECLARE_OPERATORS_FOR_FLAGS(PropertyAccessFlags)

/**
 * Returns list of the object property and dynamic property names or empty list if the object is
 * nullptr. By default QObject properties are excluded, set includeBaseProperties to true to include.
 */
template<class Base = QObject>
inline QSet<QByteArray> propertyNames(
    const QObject* object,
    PropertyAccessFlags propertyAccessFlags = PropertyAccess::anyAccess,
    bool includeBaseProperties = false,
    bool hasNotifySignal = false)
{
    static_assert(std::is_base_of<QObject, Base>());

    if (!object )
        return {};

    const auto baseMetaObject = &Base::staticMetaObject;
    const auto metaObject = object->metaObject();
    if (!NX_ASSERT(metaObject->inherits(baseMetaObject)))
        return {};

    QSet<QByteArray> result;
    const int offset = includeBaseProperties
        ? baseMetaObject->propertyOffset()
        : baseMetaObject->propertyOffset() + baseMetaObject->propertyCount();


    for (int i = offset; i < metaObject->propertyCount(); ++i)
    {
        auto property = metaObject->property(i);

        if (propertyAccessFlags.testFlag(PropertyAccess::stored) && !property.isStored())
            continue;

        if (propertyAccessFlags.testFlag(PropertyAccess::readable) && !property.isReadable())
            continue;

        if (propertyAccessFlags.testFlag(PropertyAccess::writable) && !property.isWritable())
            continue;

        if (hasNotifySignal && !property.hasNotifySignal())
            continue;

        result << metaObject->property(i).name();
    }

    for (const auto& propertyName: object->dynamicPropertyNames())
        result << propertyName;

    return result;
}

/** Returns signals, slots and invokables of the object. */
template<class Base = QObject>
inline QList<QMetaMethod> methods(const QObject* object, bool includeBaseMethods = false)
{
    static_assert(std::is_base_of<QObject, Base>());

    if (!object)
        return {};

    const auto baseMetaObject = &Base::staticMetaObject;
    const auto metaObject = object->metaObject();
    if (!NX_ASSERT(metaObject->inherits(baseMetaObject)))
        return {};

    const int offset =
        includeBaseMethods ? baseMetaObject->methodOffset() : baseMetaObject->methodCount();

    QList<QMetaMethod> result;
    for (int i = offset; i < metaObject->methodCount(); ++i)
        result << metaObject->method(i);

    return result;
}

/** Connects all the sender properties notify signals with the receiver meta method. */
void NX_UTILS_API watchOnPropertyChanges(
    QObject* sender,
    QObject* receiver,
    const QMetaMethod& receiverMetaMethod);

void NX_UTILS_API resetProperties(QObject* object);

std::optional<QMetaMethod> NX_UTILS_API findSignal(const QObject* object, QByteArrayView name);

/** Returns all the overloads with the given name, including the ones of the base classes. */
QList<QMetaMethod> NX_UTILS_API findMethods(const QMetaObject* metaObject, QByteArrayView name);

/** Whether the method is a NOTIFY signal of some property. */
bool NX_UTILS_API isNotifySignal(const QMetaObject* metaObject, const QMetaMethod& method);

} // namespace nx::utils
