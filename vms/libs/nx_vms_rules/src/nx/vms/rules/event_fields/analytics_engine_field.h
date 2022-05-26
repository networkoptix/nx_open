// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../data_macros.h"
#include "../event_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsEngineField: public EventField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.analyticsEngine")

    FIELD(QnUuid, value, setValue)

public:
    virtual bool match(const QVariant& eventValue) const override;
};

} // namespace nx::vms::rules
