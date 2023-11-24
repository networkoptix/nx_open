// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_sdk_event.h"

#include <analytics/db/text_search_utils.h>
#include <core/resource/resource.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/analytics/taxonomy/utils.h>
#include <nx/kit/utils.h>
#include <nx/utils/uuid.h>
#include <nx/vms/common/system_context.h>

namespace nx {
namespace vms {
namespace event {

using namespace nx::vms::api::analytics;
using nx::analytics::taxonomy::AbstractState;
using nx::analytics::taxonomy::AbstractEventType;
using nx::analytics::taxonomy::AbstractGroup;
using nx::analytics::taxonomy::AbstractScope;

AnalyticsSdkEvent::AnalyticsSdkEvent(
    QnResourcePtr resource,
    QnUuid engineId,
    QString eventTypeId,
    EventState toggleState,
    QString caption,
    QString description,
    nx::common::metadata::Attributes attributes,
    QnUuid objectTrackId,
    QString key,
    qint64 timeStampUsec)
    :
    base_type(EventType::analyticsSdkEvent, resource, toggleState, timeStampUsec),
    m_engineId(engineId),
    m_eventTypeId(std::move(eventTypeId)),
    m_caption(std::move(caption)),
    m_description(std::move(description)),
    m_attributes(std::move(attributes)),
    m_objectTrackId(objectTrackId),
    m_key(key)
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
    params.objectTrackId = m_objectTrackId;
    params.key = m_key;
    params.attributes = m_attributes;
    return params;
}

EventParameters AnalyticsSdkEvent::getRuntimeParamsEx(
    const EventParameters& /*ruleEventParams*/) const
{
    auto params = getRuntimeParams();
    params.caption = m_caption;
    params.description = m_description;
    return params;
}

QString AnalyticsSdkEvent::getExternalUniqueKey() const
{
    static constexpr char recordSeparator[] = "\x1E";
    const QString key =
        m_eventTypeId + recordSeparator + m_objectTrackId.toString() + recordSeparator + m_key;
    NX_VERBOSE(this, "Event's ExternalUniqueKey: %1", nx::kit::utils::toString(key.toStdString()));
    return key;
}

bool AnalyticsSdkEvent::checkEventParams(const EventParameters& params) const
{
    if (!getResource() || m_engineId != params.getAnalyticsEngineId())
        return false;

    const std::shared_ptr<AbstractState> taxonomyState =
        getResource()->systemContext()->analyticsTaxonomyState();

    if (!taxonomyState)
        return false;

    const AbstractEventType* eventType = taxonomyState->eventTypeById(m_eventTypeId);

    if (!eventType)
        return false;

    const bool isEventTypeMatched =
        m_eventTypeId == params.getAnalyticsEventTypeId()
        || nx::analytics::taxonomy::eventBelongsToGroup(
            eventType,
            params.getAnalyticsEventTypeId());

    if (!isEventTypeMatched || !checkForKeywords(m_caption, params.caption))
        return false;

    if (checkForKeywords(m_description, params.description))
        return true;

    nx::analytics::db::TextMatcher textMatcher(params.description);
    return textMatcher.matchAttributes(m_attributes);
}

const nx::common::metadata::Attributes& AnalyticsSdkEvent::attributes() const
{
    return m_attributes;
}

const std::optional<QString> AnalyticsSdkEvent::attribute(const QString& attributeName) const
{
    for (const auto& attribute: m_attributes)
    {
        if (attribute.name == attributeName)
            return attribute.value;
    }

    return std::nullopt;
}

const QnUuid AnalyticsSdkEvent::objectTrackId() const
{
    return m_objectTrackId;
}

const QString& AnalyticsSdkEvent::key() const
{
    return m_key;
}

} // namespace event
} // namespace vms
} // namespace nx
