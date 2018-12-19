#include "analytics_sdk_event.h"

#include <nx/utils/uuid.h>
#include <core/resource/resource.h>

#include <nx/analytics/descriptor_manager.h>
#include <nx/vms/api/analytics/descriptors.h>

namespace nx {
namespace vms {
namespace event {

using namespace nx::vms::api::analytics;

namespace {

bool belongsToGroup(const EventTypeDescriptor& descriptor, const QString& groupId)
{
    for (const auto& scope : descriptor.scopes)
    {
        if (scope.groupId == groupId)
            return true;
    }

    return false;
};

} // namespace

AnalyticsSdkEvent::AnalyticsSdkEvent(
    const QnResourcePtr& resource,
    const QString& pluginId,
    const QString& eventTypeId,
    EventState toggleState,
    const QString& caption,
    const QString& description,
    const QString& auxiliaryData,
    qint64 timeStampUsec)
    :
    base_type(EventType::analyticsSdkEvent, resource, toggleState, timeStampUsec),
    m_pluginId(pluginId),
    m_eventTypeId(eventTypeId),
    m_caption(caption),
    m_description(description),
    m_auxiliaryData(auxiliaryData)
{
}

QString AnalyticsSdkEvent::pluginId() const
{
    return m_pluginId;
}

const QString& AnalyticsSdkEvent::eventTypeId() const
{
    return m_eventTypeId;
}

EventParameters AnalyticsSdkEvent::getRuntimeParams() const
{
    EventParameters params = base_type::getRuntimeParams();
    params.setAnalyticsPluginId(m_pluginId);
    params.setAnalyticsEventTypeId(m_eventTypeId);
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
    if (!getResource() || m_pluginId != params.getAnalyticsPluginId())
        return false;

    nx::analytics::DescriptorManager helper(getResource()->commonModule());
    const auto descriptor = helper.eventType(m_eventTypeId);

    if (!descriptor)
        return false;

    const bool isEventTypeMatched =
        m_eventTypeId == params.getAnalyticsEventTypeId()
        || belongsToGroup(*descriptor, params.getAnalyticsEventTypeId());

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
