// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "static_web_content_info.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    StaticWebContentUpdateInfo, (json), StaticWebContentUpdateInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(StaticWebContentInfo, (json), StaticWebContentInfo_Fields)

} // namespace nx::vms::api
