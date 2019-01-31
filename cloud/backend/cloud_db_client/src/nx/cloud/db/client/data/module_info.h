#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>

#include <nx/cloud/db/api/module_info.h>

namespace nx::cloud::db::api {

#define ModuleInfo_Fields (realm)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (ModuleInfo),
    (json) )

} // namespace nx::cloud::db::api
