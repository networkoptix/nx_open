// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Integration Action Id. Displayed as a combo box with available Integration Action names. */
class NX_VMS_RULES_API IntegrationActionField:
    public SimpleTypeActionField<QString, IntegrationActionField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "integrationActionType")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<QString, IntegrationActionField>::SimpleTypeActionField;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
