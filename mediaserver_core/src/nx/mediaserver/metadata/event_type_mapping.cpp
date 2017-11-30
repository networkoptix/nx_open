#include "event_type_mapping.h"

#include <boost/bimap.hpp>

namespace nx {
namespace mediaserver {
namespace metadata {

namespace {

static const std::initializer_list<
    boost::bimap<nx::vms::event::EventType, QnUuid>::value_type> list = {
    {
        nx::vms::event::EventType::cameraInputEvent,
        QnUuid(lit("{dda94af6-c699-4b33-acfb-a164830430e7}"))
    }
};

static const boost::bimap<nx::vms::event::EventType, QnUuid> kEventTypeMapping(list.begin(), list.end());

} // namespace

QnUuid guidByEventType(nx::vms::event::EventType eventType)
{
    const auto itr = kEventTypeMapping.left.find(eventType);
    if (itr == kEventTypeMapping.left.end())
        return QnUuid();

    return itr->second;
}

nx::vms::event::EventType eventTypeByGuid(const QnUuid& eventTypeGuid)
{
    const auto itr = kEventTypeMapping.right.find(eventTypeGuid);
    if (itr == kEventTypeMapping.right.end())
        return nx::vms::event::EventType::undefinedEvent;

    return itr->second;
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
