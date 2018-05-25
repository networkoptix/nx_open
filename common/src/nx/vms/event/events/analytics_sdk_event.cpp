#include "analytics_sdk_event.h"

#include <nx/utils/uuid.h>
#include <core/resource/resource.h>

namespace nx {
namespace vms {
namespace event {

AnalyticsSdkEvent::AnalyticsSdkEvent(
    const QnResourcePtr& resource,
    const QnUuid& driverId,
    const QnUuid& eventId,
    EventState toggleState,
    const QString& caption,
    const QString& description,
    const QString& auxiliaryData,
    qint64 timeStampUsec)
    :
    base_type(EventType::analyticsSdkEvent, resource, toggleState, timeStampUsec),
    m_driverId(driverId),
    m_eventId(eventId),
    m_caption(caption),
    m_description(description),
    m_auxiliaryData(auxiliaryData)
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
    params.caption = m_caption;
    params.description = m_description;
    return params;
}

bool AnalyticsSdkEvent::checkEventParams(const EventParameters& params) const
{
    if (!getResource() || m_driverId != params.analyticsDriverId())
        return false;
    const auto descriptor = nx::vms::event::AnalyticsHelper(getResource()->commonModule())
        .eventDescriptor(m_eventId);

    const bool isEventTypeMatched = m_eventId == params.analyticsEventId()
        || descriptor.groupId == params.analyticsEventId();
    return isEventTypeMatched
        && checkForKeywords(m_caption, params.caption)
        && checkForKeywords(m_description, params.description);
}

QString AnalyticsSdkEvent::auxiliaryData() const
{
    return m_auxiliaryData;
}

} // namespace event
} // namespace vms
} // namespace nx
