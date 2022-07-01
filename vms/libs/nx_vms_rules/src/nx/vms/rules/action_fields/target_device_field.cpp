// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_device_field.h"

namespace nx::vms::rules {

QVariant TargetDeviceField::build(const AggregatedEventPtr& /*eventAggregator*/) const
{
    return QVariant::fromValue(ids());
}

} // namespace nx::vms::rules
