#pragma once

#include <nx/vms/event/actions/common_action.h>
#include <health/system_health.h>

namespace nx {
namespace vms {
namespace event {

class SystemHealthAction: public CommonAction
{
    using base_type = CommonAction;

public:
    explicit SystemHealthAction(QnSystemHealth::MessageType message,
        const QnUuid& eventResourceId = QnUuid());
};

} // namespace event
} // namespace vms
} // namespace nx
