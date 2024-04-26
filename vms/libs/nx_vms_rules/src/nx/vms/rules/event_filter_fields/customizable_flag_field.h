// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API CustomizableFlagField:
    public SimpleTypeEventField<bool, CustomizableFlagField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.customizableFlag")

    Q_PROPERTY(bool value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeEventField<bool, CustomizableFlagField>::SimpleTypeEventField;

    bool match(const QVariant& value) const override;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
