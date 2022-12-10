// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Stores HTTP method as a string. */
class NX_VMS_RULES_API HttpMethodField: public SimpleTypeActionField<QString, HttpMethodField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.httpMethod")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    HttpMethodField() = default;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
