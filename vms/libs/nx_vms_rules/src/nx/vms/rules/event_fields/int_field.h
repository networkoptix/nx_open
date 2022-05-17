// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Stores integer. Typically displayed as a spinbox. */
class NX_VMS_RULES_API IntField: public SimpleTypeEventField<int>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.int")

    Q_PROPERTY(int value READ value WRITE setValue)

public:
    IntField() = default;
};

} // namespace nx::vms::rules
