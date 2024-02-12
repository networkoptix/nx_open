// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_engine_field.h"

namespace nx::vms::rules {

bool AnalyticsEngineField::match(const QVariant& eventValue) const
{
    return value().isNull() || eventValue.value<nx::Uuid>() == value();
}

} // namespace nx::vms::rules
