#include "analytics_sdk_event.h"

#include <nx/utils/uuid.h>
#include <core/resource/resource.h>

#include <nx/analytics/event_type_descriptor_manager.h>
#include <nx/vms/api/analytics/descriptors.h>

namespace nx {
namespace vms {
namespace event {

using namespace nx::vms::api::analytics;

namespace {

bool belongsToGroup(const EventTypeDescriptor& descriptor, const QString& groupId)
{
    for (const auto& scope: descriptor.scopes)
    {
        if (scope.groupId == groupId)
            return true;
    }

    return false;
};

} // namespace

AnalyticsSdkEvent::AnalyticsSdkEvent(
    QnResourcePtr resource,
    QnUuid engineId,
    QString eventTypeId,
    EventState toggleState,
    QString caption,
    QString description,
    std::map<QString, QString> attributes,
    qint64 timeStampUsec)
    :
    base_type(EventType::analyticsSdkEvent, resource, toggleState, timeStampUsec),
    m_engineId(engineId),
    m_eventTypeId(std::move(eventTypeId)),
    m_caption(std::move(caption)),
    m_description(std::move(description)),
    m_attributes(std::move(attributes))
{
}

QnUuid AnalyticsSdkEvent::engineId() const
{
    return m_engineId;
}

const QString& AnalyticsSdkEvent::eventTypeId() const
{
    return m_eventTypeId;
}

EventParameters AnalyticsSdkEvent::getRuntimeParams() const
{
    EventParameters params = base_type::getRuntimeParams();
    params.setAnalyticsEngineId(m_engineId);
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
    if (!getResource() || m_engineId != params.getAnalyticsEngineId())
        return false;

    nx::analytics::EventTypeDescriptorManager descriptorManager(getResource()->commonModule());
    const auto descriptor = descriptorManager.descriptor(m_eventTypeId);

    if (!descriptor)
        return false;

    const bool isEventTypeMatched =
        m_eventTypeId == params.getAnalyticsEventTypeId()
        || belongsToGroup(*descriptor, params.getAnalyticsEventTypeId());

    return isEventTypeMatched
        && checkForKeywords(m_caption, params.caption)
        && checkForKeywords(m_description, params.description);
}

const std::map<QString, QString>& AnalyticsSdkEvent::attributes() const
{
    return m_attributes;
}

const std::optional<QString> AnalyticsSdkEvent::attribute(const QString& attributeName) const
{
    if (auto it = m_attributes.find(attributeName); it != m_attributes.cend())
        return it->second;

    return std::nullopt;
}

} // namespace event
} // namespace vms
} // namespace nx
