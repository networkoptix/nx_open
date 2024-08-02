// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "serialization.h"

#include <QtCore/QMetaProperty>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/qobject.h>

namespace nx::vms::rules {

QMap<QString, QJsonValue> serializeProperties(const QObject* object, const QSet<QByteArray>& names)
{
    if (!NX_ASSERT(object))
        return {};

    QMap<QString, QJsonValue> serialized;
    const auto meta = object->metaObject();

    for (const auto& propName: names)
    {
        int index = meta->indexOfProperty(propName.constData());
        if (!NX_ASSERT(index >= 0, "Property not found: %1", propName))
            continue;

        auto prop = meta->property(index);
        auto userType = prop.userType();

        if (!NX_ASSERT(
            userType != QMetaType::UnknownType,
            "Unregistered prop type: %1",
            prop.typeName()))
        {
            continue;
        }

        auto serializer = QnJsonSerializer::serializer(prop.userType());
        if (!NX_ASSERT(serializer, "Unregistered serializer for prop type: %1", prop.typeName()))
            continue;

        QJsonValue jsonValue;
        QnJsonContext ctx;

        serializer->serialize(&ctx, prop.read(object), &jsonValue);

        if (NX_ASSERT(!jsonValue.isUndefined()))
            serialized.insert(prop.name(), std::move(jsonValue));
    }

    for (const auto& propName: object->dynamicPropertyNames())
    {
        serialized.insert(propName, object->property(propName).toJsonValue());
    }

    return serialized;
}

bool deserializeProperties(const QMap<QString, QJsonValue>& propMap, QObject* object)
{
    if (!NX_ASSERT(object))
        return false;

    auto meta = object->metaObject();
    for (auto it = propMap.begin(); it != propMap.end(); ++it)
    {
        const auto& propName = it.key();
        const auto& propValue = it.value();
        auto propIndex = meta->indexOfProperty(propName.toUtf8());
        if (propIndex < 0)
        {
            NX_DEBUG(
                NX_SCOPE_TAG, "Absent property: %1 in class: %2", propName, meta->className());
            continue;
        }

        auto prop = meta->property(propIndex);
        auto userType = prop.userType();
        if (!NX_ASSERT(
            userType != QMetaType::UnknownType,
            "Unregistered prop type: %1",
            prop.typeName()))
        {
            return false;
        }

        auto serializer = QnJsonSerializer::serializer(userType);
        if (!NX_ASSERT(serializer, "Unregistered serializer for prop type: %1", prop.typeName()))
            return false;

        QnJsonContext ctx;
        QVariant propVariant;

        if (!serializer->deserialize(&ctx, propValue, &propVariant))
        {
            NX_DEBUG(NX_SCOPE_TAG, "Can't deserialize property: %1, value: %2", propName, propValue);
            return false;
        }

        if (NX_ASSERT(propVariant.isValid()))
            prop.write(object, propVariant);
    }

    return true;
}

QByteArray serialized(const QObject* object, bool storedOnly)
{
    nx::utils::PropertyAccessFlags flags = nx::utils::PropertyAccess::anyAccess;
    if (storedOnly)
        flags.setFlag(nx::utils::PropertyAccess::stored);

    return QJson::serialized(serializeProperties(object, nx::utils::propertyNames(object, flags)));
}

} // namespace nx::vms::rules
