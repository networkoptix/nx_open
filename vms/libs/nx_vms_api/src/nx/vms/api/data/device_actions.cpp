// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_actions.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DevicePasswordRequest, (json), DevicePasswordRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsFilter, (json), AnalyticsFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceFootageRequest, (json), DeviceFootageRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceDiagnosis, (json), DeviceDiagnosis_Fields)

DeprecatedFieldNames const * AnalyticsFilter::getDeprecatedFieldNames()
{
    // Since /rest/v4.
    const static DeprecatedFieldNames names{{"maxAnalyticsDetailsMs", "maxAnalyticsDetails"}};
    return &names;
}

} // namespace nx::vms::api
