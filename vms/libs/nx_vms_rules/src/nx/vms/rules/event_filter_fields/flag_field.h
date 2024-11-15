// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Stores boolean flag. Typically displayed as a checkbox. */
class NX_VMS_RULES_API EventFlagField: public SimpleTypeEventField<bool, EventFlagField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "flag")

    Q_PROPERTY(bool value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeEventField<bool, EventFlagField>::SimpleTypeEventField;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
