// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../simple_type_field.h"

namespace nx::vms::rules {

/** Stores simple text string w/o validation, e.g. email, login or comment. */
class NX_VMS_RULES_API ActionTextField: public SimpleTypeActionField<QString>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.text")

    Q_PROPERTY(QString value READ value WRITE setValue)

public:
    ActionTextField() = default;
};

} // namespace nx::vms::rules
