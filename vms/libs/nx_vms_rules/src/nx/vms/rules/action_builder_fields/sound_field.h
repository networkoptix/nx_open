// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Stores sound file name, displayed as combobox with button. */
class NX_VMS_RULES_API SoundField: public SimpleTypeActionField<QString, SoundField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.sound")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<QString, SoundField>::SimpleTypeActionField;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
