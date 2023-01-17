// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

#include <nx/vms/api/types/resource_types.h>

namespace nx::vms::rules {

/** Stores stream quality value, displayed as a combobox. */
class NX_VMS_RULES_API StreamQualityField:
    public SimpleTypeActionField<nx::vms::api::StreamQuality, StreamQualityField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.streamQuality")

    Q_PROPERTY(nx::vms::api::StreamQuality value READ value WRITE setValue NOTIFY valueChanged)

public:
    StreamQualityField() = default;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
