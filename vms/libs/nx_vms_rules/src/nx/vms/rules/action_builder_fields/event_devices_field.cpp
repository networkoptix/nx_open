// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_devices_field.h"

#include "../utils/field.h"

namespace nx::vms::rules {

QVariant EventDevicesField::build(const AggregatedEventPtr& event) const
{
    return QVariant::fromValue(utils::getDeviceIds(event));
}

} // namespace nx::vms::rules
