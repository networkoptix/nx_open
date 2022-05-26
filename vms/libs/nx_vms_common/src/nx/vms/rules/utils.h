// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/rules/common.h>

namespace nx::vms::api { enum class EventState; }

namespace nx::vms::rules {

NX_VMS_COMMON_API nx::vms::api::rules::State convertEventState(nx::vms::api::EventState eventState);

} // namespace nx::vms::api::rules
