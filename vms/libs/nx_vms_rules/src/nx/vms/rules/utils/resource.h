// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules {

struct UuidSelection;

namespace utils {

NX_VMS_RULES_API QnUserResourceSet users(
    const UuidSelection& selection,
    const common::SystemContext* context,
    bool activeOnly = false);

NX_VMS_RULES_API bool isUserSelected(
    const UuidSelection& selection,
    const common::SystemContext* context,
    nx::Uuid userId);

} // namespace utils
} // namespace nx::vms::rules
