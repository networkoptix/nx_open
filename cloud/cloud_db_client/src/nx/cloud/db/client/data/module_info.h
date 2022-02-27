// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/reflect/instrument.h>

#include <nx/cloud/db/api/module_info.h>

namespace nx::cloud::db::api {

#define ModuleInfo_Fields (realm)

NX_REFLECTION_INSTRUMENT(ModuleInfo, ModuleInfo_Fields)

QN_FUSION_DECLARE_FUNCTIONS(ModuleInfo, (json))

} // namespace nx::cloud::db::api
