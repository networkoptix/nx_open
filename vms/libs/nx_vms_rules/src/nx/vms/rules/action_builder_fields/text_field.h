// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Stores simple text string w/o validation, e.g. email, login or comment. */
class NX_VMS_RULES_API ActionTextField: public SimpleTypeActionField<QString, ActionTextField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.text")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<QString, ActionTextField>::SimpleTypeActionField;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
