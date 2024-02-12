// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_health/message_type.h>
#include <nx/vms/event/actions/common_action.h>

namespace nx::vms::event {

class NX_VMS_COMMON_API SystemHealthAction: public CommonAction
{
    using base_type = CommonAction;

public:
    explicit SystemHealthAction(
        common::system_health::MessageType message,
        const nx::Uuid& eventResourceId = nx::Uuid(),
        const nx::common::metadata::Attributes& attributes = {});
};

} // namespace nx::vms::event
