// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Stores password text. Should be displayed by password edit widget. */
class NX_VMS_RULES_API PasswordField: public SimpleTypeActionField<QString>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.password")

    Q_PROPERTY(QString value READ value WRITE setValue)

public:
    PasswordField() = default;
};

} // namespace nx::vms::rules
