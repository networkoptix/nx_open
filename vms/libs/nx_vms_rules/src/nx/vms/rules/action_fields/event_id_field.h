// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../action_field.h"

namespace nx::vms::rules {

/**
 * Unique id of event, used for action deduplication, e.g. notification.
 */
class NX_VMS_RULES_API EventIdField: public ActionField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.eventId")

public:
    EventIdField() = default;

    QVariant build(const AggregatedEventPtr& event) const override;
};

} // namespace nx::vms::rules
