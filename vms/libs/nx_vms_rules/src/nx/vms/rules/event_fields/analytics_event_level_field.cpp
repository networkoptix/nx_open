// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_event_level_field.h"

namespace nx::vms::rules {

bool AnalyticsEventLevelField::match(const QVariant& eventValue) const
{
    return m_levels & eventValue.value<nx::vms::api::EventLevel>();
}

} // namespace nx::vms::rules
