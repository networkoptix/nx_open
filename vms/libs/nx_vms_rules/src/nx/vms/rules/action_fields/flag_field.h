// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "simple_type_field.h"

namespace nx::vms::rules {

/** Stores boolean flag. Typically displayed as a checkbox. */
class NX_VMS_RULES_API FlagField: public SimpleTypeField<bool>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.flag")

    Q_PROPERTY(bool value READ value WRITE setValue)

public:
    FlagField() = default;
};

} // namespace nx::vms::rules
