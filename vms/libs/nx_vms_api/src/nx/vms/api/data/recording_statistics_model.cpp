// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recording_statistics_model.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    RecordingStatisticsFilter, (json), RecordingStatisticsFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    RecordingStatisticsData, (json), RecordingStatisticsData_Fields)

} // namespace nx::vms::api
