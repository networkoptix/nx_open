// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "secure_cookie_storage_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CookieName, (json), CookieName_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EncodeValue, (json), EncodeValue_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DecodedValue, (json), DecodedValue_Fields)

} // namespace nx::vms::api
