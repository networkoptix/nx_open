// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Stores camera input port. Displayed with combobox. Empty value matches any input port. */
class NX_VMS_RULES_API InputPortField: public SimpleTypeEventField<QString, InputPortField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.inputPort")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    InputPortField() = default;

    bool match(const QVariant& eventValue) const override;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
