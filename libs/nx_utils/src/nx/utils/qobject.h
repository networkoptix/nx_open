// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>

#include <QtCore/QObject>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>

namespace nx::utils {

/**
 * Returns list of the object property and dynamic property names or empty list if the object is
 * nullptr. By default QObject properties are excluded, set includeBaseProperties to true to include.
 */
template<class Base = QObject>
inline QStringList propertyNames(QObject* object, bool includeBaseProperties = false)
{
    static_assert(std::is_base_of<QObject, Base>());

    if (!object )
        return {};

    QStringList result;

    const int offset = includeBaseProperties
        ? Base::staticMetaObject.propertyOffset()
        : Base::staticMetaObject.propertyOffset() + Base::staticMetaObject.propertyCount();

    auto metaObject = object->metaObject();
    for (int i = offset; i < metaObject->propertyCount(); ++i)
        result << metaObject->property(i).name();

    for (const auto& propertyName: object->dynamicPropertyNames())
        result << propertyName;

    return result;
}

} // namespace nx::utils
