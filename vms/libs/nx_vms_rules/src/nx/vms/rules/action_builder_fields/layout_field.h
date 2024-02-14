// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Stores single layout. Displayed as a layout selection button. */
class NX_VMS_RULES_API LayoutField: public SimpleTypeActionField<nx::Uuid, LayoutField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.layout")

    Q_PROPERTY(nx::Uuid value READ value WRITE setValue NOTIFY valueChanged)

public:
    LayoutField() = default;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
