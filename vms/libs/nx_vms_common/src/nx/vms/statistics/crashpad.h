// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>

namespace nx::vms::statistics {

NX_VMS_COMMON_API bool initCrashpad(bool enableUploads);
NX_VMS_COMMON_API nx::Uuid getCrashpadClientId();

} // namespace nx::vms::statistics
