// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_advanced_parameters_model.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceAdvancedFilter, (json), DeviceAdvancedFilter_Fields)

QString DeviceAdvancedFilter::toString() const
{
    return NX_FMT("deviceId: %1, id: %2", deviceId, id);
}

} // namespace nx::vms::api
