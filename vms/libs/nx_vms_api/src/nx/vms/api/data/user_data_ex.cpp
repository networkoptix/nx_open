// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_data_ex.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UserDataEx, (csv_record)(json)(ubjson)(xml), UserDataEx_Fields, (brief, true))

} // namespace nx::vms::api
