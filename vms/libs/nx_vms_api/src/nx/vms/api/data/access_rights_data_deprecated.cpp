// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_rights_data_deprecated.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AccessRightsDataDeprecated, (ubjson)(json), AccessRightsDataDeprecated_Fields)

} // namespace nx::vms::api
