// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_actions.h"

#include <nx/fusion/model_functions.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/json/qjson.h>
#include <nx/utils/json/qt_core_types.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    RequestAvailableAnalyticsActions, (json), RequestAvailableAnalyticsActions_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AvailableAnalyticsActions, (json), AvailableAnalyticsActions_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsAction, (json), AnalyticsAction_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsActionResult, (json), AnalyticsActionResult_Fields)

DeprecatedFieldNames const * AnalyticsAction::getDeprecatedFieldNames()
{
    // Since /rest/v4.
    const static DeprecatedFieldNames names{{"parameters", "params"}};
    return &names;
}

} // namespace nx::vms::api::analytics
