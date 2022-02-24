// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_failure_event.h"

namespace nx {
namespace vms {
namespace event {

ServerFailureEvent::ServerFailureEvent(
    const QnResourcePtr& resource,
    qint64 timeStamp,
    EventReason reasonCode,
    const QString& reasonText)
    :
    base_type(EventType::serverFailureEvent, resource, timeStamp, reasonCode, reasonText)
{
}

} // namespace event
} // namespace vms
} // namespace nx
