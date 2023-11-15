// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "scripts_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    RunProcessResult, (json), RunProcessResult_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ScriptInfo, (json), ScriptInfo_Fields)

} // namespace nx::vms::api
