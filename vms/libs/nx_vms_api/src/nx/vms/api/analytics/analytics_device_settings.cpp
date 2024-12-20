// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_device_settings.h"

#include <nx/fusion/model_functions.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/json/qjson.h>
#include <nx/utils/json/qt_core_types.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsDeviceSettings, (json), AnalyticsDeviceSettings_Fields);

} // namespace ns::vms::api
