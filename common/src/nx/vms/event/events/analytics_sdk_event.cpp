#include "analytics_sdk_event.h"

#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace event {

AnalyticsSdkEvent::AnalyticsSdkEvent(
    const QnResourcePtr& resource,
    const QnUuid& driverId,
    const QnUuid& eventId,
    EventState toggleState,
    qint64 timeStampUsec)
    :
    base_type(analyticsSdkEvent, resource, toggleState, timeStampUsec),
    m_driverId(driverId),
    m_eventId(eventId)
{
}

const QnUuid& AnalyticsSdkEvent::driverId() const
{
    return m_driverId;
}

const QnUuid& AnalyticsSdkEvent::eventId() const
{
    return m_eventId;
}

EventParameters AnalyticsSdkEvent::getRuntimeParams() const
{
    EventParameters params = base_type::getRuntimeParams();
    params.setAnalyticsDriverId(m_driverId);
    params.setAnalyticsEventId(m_eventId);
    return params;
}

EventParameters AnalyticsSdkEvent::getRuntimeParamsEx(
    const EventParameters& ruleEventParams) const
{
    auto params = getRuntimeParams();
    params.caption = ruleEventParams.caption;
    params.description = ruleEventParams.description;
    return params;
}

bool AnalyticsSdkEvent::checkEventParams(const EventParameters& params) const
{
    return m_driverId == params.analyticsDriverId()
        && m_eventId == params.analyticsEventId();
}

} // namespace event
} // namespace vms
} // namespace nx
