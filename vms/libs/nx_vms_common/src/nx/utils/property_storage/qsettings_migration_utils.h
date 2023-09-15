// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSettings>

#include <nx/reflect/json.h>
#include <nx/utils/log/log.h>
#include <nx/utils/property_storage/property.h>

namespace nx::utils::property_storage {

template<typename T>
void migrateValue(QSettings* oldSettings, Property<T>& property, const QString& customName = {})
{
    if (property.exists())
        return;

    auto name = customName.isNull() ? property.name : customName;
    if (oldSettings->contains(name))
        property = oldSettings->value(name).template value<T>();
}

template<typename T>
void migrateSerializedValue(
    QSettings* oldSettings, Property<T>& property, const QString& customName = {})
{
    if (property.exists())
        return;

    auto name = customName.isNull() ? property.name : customName;
    if (!oldSettings->contains(name))
        return;

    const auto serializedValue = oldSettings->value(name).toString();
    const auto [value, result] = nx::reflect::json::deserialize<T>(
        serializedValue.toStdString());

    if (result)
    {
        property = value;
    }
    else
    {
        NX_WARNING(NX_SCOPE_TAG,
            "Cannot deserialize JSON value from old settings: %1: %2. Error: %3",
            name, serializedValue, result.errorDescription);
    }
}

template<typename T>
void migrateEnumValue(
    QSettings* oldSettings, Property<T>& property, const QString& customName = {})
{
    if (property.exists())
        return;

    auto name = customName.isNull() ? property.name : customName;
    if (!oldSettings->contains(name))
        return;

    const auto serializedValue = oldSettings->value(name).toString();
    bool ok = false;
    const T value = nx::reflect::fromString<T>(serializedValue.toStdString(), &ok);

    if (ok)
    {
        property = value;
    }
    else
    {
        NX_WARNING(NX_SCOPE_TAG, "Cannot deserialize enum value from old settings: %1: %2",
            name, serializedValue);
    }
};

} // namespace nx::utils::property_storage
