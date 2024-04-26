// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/**
 * Stores simple text string w/o validation, e.g. email or login.
 * Matches any string if value is empty.
 */
class NX_VMS_RULES_API EventTextField: public SimpleTypeEventField<QString, EventTextField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.text")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeEventField<QString, EventTextField>::SimpleTypeEventField;

    bool match(const QVariant& value) const override;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
