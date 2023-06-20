// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(StorageInitResult,
    ok,
    createFailed,
    wrongPath,
    wrongAuth
)

} // namespace nx::vms::api
