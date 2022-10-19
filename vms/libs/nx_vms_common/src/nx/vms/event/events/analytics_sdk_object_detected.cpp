// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_sdk_object_detected.h"

#include <nx/kit/utils.h>
#include <nx/utils/uuid.h>
#include <core/resource/resource.h>

#include <nx/analytics/object_type_descriptor_manager.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <analytics/db/text_search_utils.h>
#include <analytics/db/analytics_db_types.h>
#include <nx/analytics/taxonomy/helpers.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>

namespace nx::vms:: event
{

using namespace nx::vms::api::analytics;

AnalyticsSdkObjectDetected::AnalyticsSdkObjectDetected(
    const QnResourcePtr& resource,
    const nx::common::metadata::ObjectMetadataPacketPtr& packet,
    const nx::common::metadata::ObjectMetadata& metadata)
    :
    base_type(EventType::analyticsSdkObjectDetected, resource, packet->timestampUs),
    m_packet(packet),
    m_metadata(metadata)
{
}

QnUuid AnalyticsSdkObjectDetected::engineId() const
{
    return m_metadata.analyticsEngineId;
}

const QString& AnalyticsSdkObjectDetected::objectTypeId() const
{
    return m_metadata.typeId;
}

EventParameters AnalyticsSdkObjectDetected::getRuntimeParams() const
{
    EventParameters params = base_type::getRuntimeParams();
    params.setAnalyticsEngineId(m_metadata.analyticsEngineId);
    params.setAnalyticsObjectTypeId(m_metadata.typeId);
    params.objectTrackId = m_metadata.trackId;
    params.attributes = m_metadata.attributes;
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
    return m_metadata.trackId.toString();
}

bool AnalyticsSdkObjectDetected::checkEventParams(const EventParameters& params) const
{
    if (!getResource())
        return false;

    const auto ruleTypeId = params.getAnalyticsObjectTypeId();

    const bool isObjectTypeMatched = ruleTypeId == m_metadata.typeId
        || nx::analytics::taxonomy::isBaseType(
            getResource()->commonModule()->taxonomyStateWatcher()->state().get(),
            ruleTypeId, m_metadata.typeId);
    if (!isObjectTypeMatched)
        return false;

    nx::analytics::db::TextMatcher textMatcher;
    textMatcher.parse(params.description);
    textMatcher.matchAttributes(m_metadata.attributes);
    return textMatcher.matched();
}

const nx::common::metadata::Attributes& AnalyticsSdkObjectDetected::attributes() const
{
    return m_metadata.attributes;
}

const std::optional<QString> AnalyticsSdkObjectDetected::attribute(const QString& attributeName) const
{
    for (const auto& attribute: m_metadata.attributes)
    {
        if (attribute.name == attributeName)
            return attribute.value;
    }

    return std::nullopt;
}

} // namespace nx::vms:: event
