// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "system_health_action.h"

namespace nx::vms::event {

class NX_VMS_COMMON_API IntercomCallAction: public SystemHealthAction
{
    using base_type = SystemHealthAction;

public:
    explicit IntercomCallAction(
        common::system_health::MessageType message,
        nx::Uuid eventResourceId = {},
        const nx::common::metadata::Attributes& attributes = {});
};

} // namespace nx::vms::event
