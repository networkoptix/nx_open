// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules {

struct UuidSelection;

namespace utils {

QnUserResourceSet users(
    const UuidSelection& selection,
    common::SystemContext* context,
    bool activeOnly = false);

} // namespace utils
} // namespace nx::vms::rules
