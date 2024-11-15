// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Stores camera input port. Displayed with combobox. Empty value matches any input port. */
class NX_VMS_RULES_API InputPortField: public SimpleTypeEventField<QString, InputPortField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "inputPort")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeEventField<QString, InputPortField>::SimpleTypeEventField;

    bool match(const QVariant& eventValue) const override;

    static QJsonObject openApiDescriptor(const QVariantMap& properties);

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
