// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/http_types.h>

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Stores HTTP auth type. Displayed as a combobox. */
class NX_VMS_RULES_API HttpAuthTypeField:
    public SimpleTypeActionField<nx::network::http::AuthType, HttpAuthTypeField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.httpAuthType")

    Q_PROPERTY(nx::network::http::AuthType value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<network::http::AuthType, HttpAuthTypeField>::SimpleTypeActionField;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
