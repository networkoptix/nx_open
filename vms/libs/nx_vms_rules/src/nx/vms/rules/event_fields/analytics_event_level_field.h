// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/event_rule_types.h>

#include "../data_macros.h"
#include "../event_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsEventLevelField: public EventField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.analyticsEventLevel")

    FIELD(nx::vms::api::EventLevels, levels, setLevels)

public:
    virtual bool match(const QVariant& eventValue) const override;
};

} // namespace nx::vms::rules
