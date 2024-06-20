// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../rule.h"

namespace nx::vms::rules::utils {

NX_VMS_RULES_API ValidationResult visibleFieldsValidity(
    const Rule* rule, common::SystemContext* context);

} // namespace nx::vms::rules::utils
