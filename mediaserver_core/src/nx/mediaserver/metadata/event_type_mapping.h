#pragma once

#include <nx/vms/event/event_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace mediaserver {
namespace metadata {

QnUuid guidByEventType(nx::vms::api::EventType eventType);

nx::vms::api::EventType eventTypeByGuid(const QnUuid& eventTypeGuid);

} // namespace metadata
} // namespace mediaserver
} // namespace nx
