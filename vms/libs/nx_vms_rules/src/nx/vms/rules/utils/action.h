// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../rules_fwd.h"

namespace nx::vms::rules {

NX_VMS_RULES_API bool isProlonged(const Engine* engine, const ActionBuilder* builder);

/** Returns whether logging is allowed for the given action. */
NX_VMS_RULES_API bool isLoggingAllowed(const Engine* engine, const ActionPtr& action);

} // namespace nx::vms::rules
