// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/json/flags.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(CloudStorageCapability,
    archive = 1,
    analytics = 2,
    bookmarks = 4,
    motion = 8
)

Q_DECLARE_FLAGS(CloudStorageCapabilities, CloudStorageCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(CloudStorageCapabilities)

} // namespace nx::vms::api
