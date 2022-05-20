// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/actions/common_action.h>
#include <health/system_health.h>

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API SystemHealthAction: public CommonAction
{
    using base_type = CommonAction;

public:
    explicit SystemHealthAction(QnSystemHealth::MessageType message,
        const QnUuid& eventResourceId = QnUuid(),
        const nx::common::metadata::Attributes& attributes = {});
};

} // namespace event
} // namespace vms
} // namespace nx
