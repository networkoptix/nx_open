// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../action_builder_field.h"

namespace nx::vms::rules {

/** Collects device ids from the event. Invisible. */
class NX_VMS_RULES_API EventDevicesField: public ActionBuilderField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.eventDevices")

public:
    using ActionBuilderField::ActionBuilderField;

    QVariant build(const AggregatedEventPtr& eventAggregator) const override;
};

} // namespace nx::vms::rules
