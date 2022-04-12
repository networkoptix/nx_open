// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_sdk_object_detected.h"

#include <nx/kit/utils.h>
#include <nx/utils/uuid.h>
#include <core/resource/resource.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <analytics/db/text_search_utils.h>
#include <analytics/db/analytics_db_types.h>
#include <nx/analytics/taxonomy/helpers.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>

namespace nx::vms:: event
{

using namespace nx::vms::api::analytics;

AnalyticsSdkObjectDetected::AnalyticsSdkObjectDetected(
    QnResourcePtr resource,
    QnUuid engineId,
    QString objectTypeId,
    nx::common::metadata::Attributes attributes,
    QnUuid objectTrackId,
    qint64 timeStampUsec)
    :
    base_type(EventType::analyticsSdkObjectDetected, resource, timeStampUsec),
    m_engineId(engineId),
    m_objectTypeId(std::move(objectTypeId)),
    m_attributes(std::move(attributes)),
    m_objectTrackId(objectTrackId)
{
}

QnUuid AnalyticsSdkObjectDetected::engineId() const
{
    return m_engineId;
}

const QString& AnalyticsSdkObjectDetected::objectTypeId() const
{
    return m_objectTypeId;
}

QnUuid AnalyticsSdkObjectDetected::objectTrackId() const
{
    return m_objectTrackId;
}

EventParameters AnalyticsSdkObjectDetected::getRuntimeParams() const
{
    EventParameters params = base_type::getRuntimeParams();
    params.setAnalyticsEngineId(m_engineId);
    params.setAnalyticsObjectTypeId(m_objectTypeId);
    params.objectTrackId = m_objectTrackId;
    params.attributes = m_attributes;
    return params;
}

EventParameters AnalyticsSdkObjectDetected::getRuntimeParamsEx(
    const EventParameters& /*ruleEventParams*/) const
{
    auto params = getRuntimeParams();
    return params;
}

QString AnalyticsSdkObjectDetected::getExternalUniqueKey() const
{
    return m_objectTrackId.toString();
}

bool AnalyticsSdkObjectDetected::checkEventParams(const EventParameters& params) const
{
    if (!getResource())
        return false;

    const auto ruleTypeId = params.getAnalyticsObjectTypeId();

    const bool isObjectTypeMatched = ruleTypeId == m_objectTypeId
        || nx::analytics::taxonomy::isBaseType(
            getResource()->systemContext()->analyticsTaxonomyState().get(),
            ruleTypeId, m_objectTypeId);
    if (!isObjectTypeMatched)
        return false;

    nx::analytics::db::TextMatcher textMatcher;
    textMatcher.parse(params.description);
    textMatcher.matchAttributes(m_attributes);
    return textMatcher.matched();
}

const nx::common::metadata::Attributes& AnalyticsSdkObjectDetected::attributes() const
{
    return m_attributes;
}

const std::optional<QString> AnalyticsSdkObjectDetected::attribute(const QString& attributeName) const
{
    for (const auto& attribute: m_attributes)
    {
        if (attribute.name == attributeName)
            return attribute.value;
    }

    return std::nullopt;
}

} // namespace nx::vms:: event
