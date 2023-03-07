// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_models.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT(ResourceGroup, ResourceGroup_Fields)
QN_FUSION_DEFINE_FUNCTIONS(ResourceGroup, (json))

} // namespace nx::vms::api
