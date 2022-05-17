// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"
#include "../rules_fwd.h"

namespace nx::vms::rules {

/** Stores event state. Typically displayed as a combobox. */
class NX_VMS_RULES_API StateField: public SimpleTypeEventField<State>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.eventState")

    Q_PROPERTY(nx::vms::api::rules::State value READ value WRITE setValue)

public:
    StateField() = default;

    bool match(const QVariant& eventValue) const override;
};

} // namespace nx::vms::rules
