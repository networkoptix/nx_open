#pragma once

#include <nx/vms/event/event_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace mediaserver {
namespace metadata {

QnUuid guidByEventType(nx::vms::event::EventType eventType);

nx::vms::event::EventType eventTypeByGuid(const QnUuid& eventTypeGuid);

} // namespace metadata
} // namespace mediaserver
} // namespace nx
