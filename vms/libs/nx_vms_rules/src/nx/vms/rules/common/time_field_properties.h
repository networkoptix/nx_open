// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QVariant>

namespace nx::vms::rules {

struct TimeFieldProperties
{
    /* Value that used by the action builder to make a field. */
    std::chrono::seconds value = std::chrono::seconds::zero();

    /* Value that used by the OptionalTimeField on switching from 'no duration' to 'has duration'. */
    std::chrono::seconds defaultValue = value;

    /* Maximum duration value. */
    std::chrono::seconds maximumValue = std::chrono::weeks{1};

    /* Minimum duration value. */
    std::chrono::seconds minimumValue = std::chrono::seconds::zero();

    QVariantMap toVariantMap() const
    {
        return QVariantMap{
            {"value", QVariant::fromValue(value)},
            {"default", QVariant::fromValue(defaultValue)},
            {"min", QVariant::fromValue(minimumValue)},
            {"max", QVariant::fromValue(maximumValue)}};
    }

    static TimeFieldProperties fromVariantMap(const QVariantMap& properties)
    {
        TimeFieldProperties result;

        if (properties.contains("value"))
            result.value = properties.value("value").value<std::chrono::seconds>();

        if (properties.contains("default"))
            result.defaultValue = properties.value("default").value<std::chrono::seconds>();

        if (properties.contains("min"))
            result.minimumValue = properties.value("min").value<std::chrono::seconds>();

        if (properties.contains("max"))
            result.maximumValue = properties.value("max").value<std::chrono::seconds>();

        return result;
    }
};

} // namespace nx::vms::rules
