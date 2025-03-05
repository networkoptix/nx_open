// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_forecast_model.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(StorageForecastData, (json), StorageForecastData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(StorageForecastV2, (json), StorageForecastV2_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(StorageForecastV4, (json), StorageForecastV4_Fields)

} // namespace nx::vms::api
