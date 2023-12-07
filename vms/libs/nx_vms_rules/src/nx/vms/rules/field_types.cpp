// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "field_types.h"

#include <nx/fusion/model_functions.h>

namespace nx::network::http {
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SerializableCredentials, (json), SerializableCredentials_Fields)
} // namespace nx::network::http

namespace nx::vms::rules {
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(UuidSelection, (json), UuidSelection_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AuthenticationInfo, (json), AuthenticationInfo_Fields)
} // namespace nx::vms::rules
