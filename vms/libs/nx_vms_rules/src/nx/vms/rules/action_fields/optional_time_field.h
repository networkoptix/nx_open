// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/metatypes.h>

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/**
 * Action field for storing optional time value. Typically displayed with a special editor widget.
 * Stores positive value in seconds if the editor checkbox is checked, zero otherwise.
 */
class NX_VMS_RULES_API OptionalTimeField: public SimpleTypeActionField<std::chrono::seconds>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.optionalTime")

    Q_PROPERTY(std::chrono::seconds value READ value WRITE setValue)

public:
    OptionalTimeField() = default;
};

} // namespace nx::vms::rules
