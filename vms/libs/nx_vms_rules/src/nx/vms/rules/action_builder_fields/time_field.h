// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/metatypes.h>

#include "../base_fields/simple_type_field.h"
#include "../common/time_field_properties.h"
#include "../manifest.h"

namespace nx::vms::rules {

/**
 * Action builder field for storing time value.
 * Typically displayed with a spinbox edit with time unit combo.
 * Stores non-negative value in microseconds.
 */
class NX_VMS_RULES_API TimeField:
    public SimpleTypeActionField<std::chrono::microseconds, TimeField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.time")

    Q_PROPERTY(std::chrono::microseconds value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<std::chrono::microseconds, TimeField>::SimpleTypeActionField;

    TimeFieldProperties properties() const
    {
        return TimeFieldProperties::fromVariantMap(descriptor()->properties);
    }

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
