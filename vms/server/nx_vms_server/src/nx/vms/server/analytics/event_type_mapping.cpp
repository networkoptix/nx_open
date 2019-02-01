#include "event_type_mapping.h"

#include <boost/bimap.hpp>

namespace nx {
namespace vms::server {
namespace analytics {

namespace {

static const std::initializer_list<
    boost::bimap<nx::vms::api::EventType, QString>::value_type> list = {
    {
        nx::vms::api::EventType::cameraInputEvent,
        "nx.hanwha.AlarmInput"
    }
};

static const boost::bimap<nx::vms::api::EventType, QString> kEventTypeMapping(
    list.begin(), list.end());

} // namespace

QString eventTypeIdByEventType(nx::vms::api::EventType eventType)
{
    const auto it = kEventTypeMapping.left.find(eventType);
    if (it == kEventTypeMapping.left.end())
        return QString();

    return it->second;
}

nx::vms::api::EventType eventTypeByEventTypeId(const QString& eventTypeId)
{
    const auto it = kEventTypeMapping.right.find(eventTypeId);
    if (it == kEventTypeMapping.right.end())
        return nx::vms::api::EventType::undefinedEvent;

    return it->second;
}

} // namespace analytics
} // namespace vms::server
} // namespace nx
