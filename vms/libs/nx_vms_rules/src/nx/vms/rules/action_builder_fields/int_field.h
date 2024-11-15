// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"
#include "../data_macros.h"

namespace nx::vms::rules {

/** Stores integer value, displayed with a spinbox. */
class NX_VMS_RULES_API ActionIntField: public SimpleTypeActionField<int, ActionIntField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "int")

    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    FIELD(int, min, setMin)
    FIELD(int, max, setMax)

public:
    using SimpleTypeActionField<int, ActionIntField>::SimpleTypeActionField;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
