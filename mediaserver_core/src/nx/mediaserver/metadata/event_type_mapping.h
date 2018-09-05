#pragma once

#include <nx/vms/event/event_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace mediaserver {
namespace metadata {

QString eventTypeIdByEventType(nx::vms::api::EventType eventType);

nx::vms::api::EventType eventTypeByEventTypeId(const QString& eventTypeId);

} // namespace metadata
} // namespace mediaserver
} // namespace nx
