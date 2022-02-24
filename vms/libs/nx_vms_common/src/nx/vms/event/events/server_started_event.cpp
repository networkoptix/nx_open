// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_started_event.h"

namespace nx {
namespace vms {
namespace event {

ServerStartedEvent::ServerStartedEvent(const QnResourcePtr& resource, qint64 timeStamp):
    base_type(EventType::serverStartEvent, resource, timeStamp)
{
}

} // namespace event
} // namespace vms
} // namespace nx
