// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/event_rule_types.h>

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsEventLevelField:
    public SimpleTypeEventField<nx::vms::api::EventLevels, AnalyticsEventLevelField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.analyticsEventLevel")

    Q_PROPERTY(nx::vms::api::EventLevels value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeEventField<vms::api::EventLevels, AnalyticsEventLevelField>::SimpleTypeEventField;

    virtual bool match(const QVariant& eventValue) const override;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
