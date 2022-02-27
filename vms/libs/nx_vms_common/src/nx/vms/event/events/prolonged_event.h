// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/events/abstract_event.h>

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API ProlongedEvent: public AbstractEvent
{
    using base_type = AbstractEvent;

protected:
    explicit ProlongedEvent(EventType eventType, const QnResourcePtr& resource,
        EventState toggleState, qint64 timeStamp);

public:
    virtual bool isEventStateMatched(EventState state, ActionType actionType) const override;
};

} // namespace event
} // namespace vms
} // namespace nx
