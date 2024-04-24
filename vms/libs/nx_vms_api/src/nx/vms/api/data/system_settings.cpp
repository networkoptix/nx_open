// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_settings.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SaveableSystemSettings, (json), SaveableSystemSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemSettings, (json), SystemSettings_Fields)

} // namespace nx::vms::api
