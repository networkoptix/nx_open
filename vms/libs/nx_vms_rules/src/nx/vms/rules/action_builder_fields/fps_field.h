// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "int_field.h"

namespace nx::vms::rules {

/**
 * Stores frames-per-second for recording Devices.
 * Max value is updated dynamically, depending on a selected Device.
 * Displayed as a spinbox.
 */
class NX_VMS_RULES_API FpsField: public SimpleTypeActionField<int, FpsField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.fps")

    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)

public:
    FpsField() = default;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
