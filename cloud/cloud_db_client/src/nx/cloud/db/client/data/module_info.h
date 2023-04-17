// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/module_info.h>
#include <nx/reflect/instrument.h>

namespace nx::cloud::db::api {

#define ModuleInfo_Fields (realm)

NX_REFLECTION_INSTRUMENT(ModuleInfo, ModuleInfo_Fields)

} // namespace nx::cloud::db::api
