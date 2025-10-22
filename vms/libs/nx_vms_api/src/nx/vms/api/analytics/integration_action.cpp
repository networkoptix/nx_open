// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_action.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(FieldModel, (json), FieldModel_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(IntegrationAction, (json),
    IntegrationAction_Fields, (brief, true))

} // namespace nx::vms::api::analytics
