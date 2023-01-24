// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Stores selected layout ids. Displayed as a layout selection button. */
class NX_VMS_RULES_API TargetLayoutField: public SimpleTypeActionField<QnUuidSet, TargetLayoutField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.targetLayout")

    Q_PROPERTY(QnUuidSet value READ value WRITE setValue NOTIFY valueChanged)

public:
    TargetLayoutField() = default;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
