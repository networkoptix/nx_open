// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/metatypes.h>

#include "../base_fields/simple_type_field.h"
#include "../manifest.h"

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

/**
 * Action field for storing optional time value. Typically displayed with a special editor widget.
 * Stores positive value in microseconds if the editor checkbox is checked, zero otherwise.
 */
class NX_VMS_RULES_API OptionalTimeField:
    public SimpleTypeActionField<std::chrono::microseconds, OptionalTimeField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.optionalTime")

    Q_PROPERTY(std::chrono::microseconds value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<std::chrono::microseconds, OptionalTimeField>::SimpleTypeActionField;

    TimeFieldProperties properties() const
    {
        return TimeFieldProperties::fromVariantMap(descriptor()->properties);
    }

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
