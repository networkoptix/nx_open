// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API ExpectedUuidField: public SimpleTypeEventField<nx::Uuid, ExpectedUuidField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "expectedUuid")

    Q_PROPERTY(nx::Uuid value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeEventField<nx::Uuid, ExpectedUuidField>::SimpleTypeEventField;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
