// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QVariant>

namespace nx::vms::rules {

struct TimeFieldProperties
{
    /* Value that used by the action builder to make a field. */
    std::chrono::seconds value = std::chrono::seconds::zero();

    /* Maximum duration value. */
    std::chrono::seconds maximumValue = std::chrono::weeks{1};

    /**
     * Minimum duration value. Even if the `minimumValue` is greater than zero, the `value`
     * can be set to zero if the given structure is used for an OptionalTimeField, for which zero
     * means the absence of a value.
     */
    std::chrono::seconds minimumValue = std::chrono::seconds::zero();

    QVariantMap toVariantMap() const
    {
        return QVariantMap{
            {"value", QVariant::fromValue(value)},
            {"min", QVariant::fromValue(minimumValue)},
            {"max", QVariant::fromValue(maximumValue)}};
    }

    static std::chrono::seconds secondsFromVariant(const QVariant& variant)
    {
        if (variant.canConvert<std::chrono::seconds>())
            return variant.value<std::chrono::seconds>();

        if (variant.canConvert<int>())
            return std::chrono::seconds(variant.toInt());

        NX_ASSERT(false, "TimeFieldProperties: Invalid variant type for seconds conversion.");
        return std::chrono::seconds::zero();
    }

    static TimeFieldProperties fromVariantMap(const QVariantMap& properties)
    {
        TimeFieldProperties result;

        if (properties.contains("value"))
            result.value = secondsFromVariant(properties.value("value"));

        if (properties.contains("min"))
            result.minimumValue = secondsFromVariant(properties.value("min"));

        if (properties.contains("max"))
            result.maximumValue = secondsFromVariant(properties.value("max"));

        return result;
    }
};

} // namespace nx::vms::rules
