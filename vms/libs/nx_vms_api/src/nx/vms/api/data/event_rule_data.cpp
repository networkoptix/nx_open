// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_rule_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EventRuleData, (ubjson)(xml)(json)(sql_record)(csv_record), EventRuleData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EventActionData, (ubjson)(xml)(json)(sql_record)(csv_record), EventActionData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ResetEventRulesData,
    (ubjson)(xml)(json)(sql_record)(csv_record), ResetEventRulesData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
