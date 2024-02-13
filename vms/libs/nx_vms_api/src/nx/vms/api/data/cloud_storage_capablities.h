// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/serialization/flags.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(CloudStorageCapability,
    archive,
    analytics,
    bookmarks,
    motion
)

Q_DECLARE_FLAGS(CloudStorageCapabilities, CloudStorageCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(CloudStorageCapabilities)

} // namespace nx::vms::api
