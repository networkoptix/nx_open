// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_failure_event.h"

#include <core/resource/resource.h>

namespace nx {
namespace vms {
namespace event {

StorageFailureEvent::StorageFailureEvent(
    const QnResourcePtr& resource,
    qint64 timeStamp,
    EventReason reasonCode,
    const QString& storageUrl)
    :
    base_type(EventType::storageFailureEvent, resource, timeStamp, reasonCode, storageUrl)
{
}

} // namespace event
} // namespace vms
} // namespace nx
