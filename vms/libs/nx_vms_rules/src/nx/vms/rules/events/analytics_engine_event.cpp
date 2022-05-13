// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_engine_event.h"

#include <nx/analytics/taxonomy/abstract_engine.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/vms/common/system_context.h>

#include "../utils/event_details.h"

namespace nx::vms::rules {

AnalyticsEngineEvent::AnalyticsEngineEvent(
    std::chrono::microseconds timestamp,
    const QString& caption,
    const QString& description,
    QnUuid cameraId,
    QnUuid engineId)
    :
    DescribedEvent(timestamp, caption, description),
    m_cameraId(cameraId),
    m_engineId(engineId)
{
}

QnUuid AnalyticsEngineEvent::cameraId() const
{
    return m_cameraId;
}

void AnalyticsEngineEvent::setCameraId(QnUuid cameraId)
{
    m_cameraId = cameraId;
}

QnUuid AnalyticsEngineEvent::engineId() const
{
    return m_engineId;
}

void AnalyticsEngineEvent::setEngineId(QnUuid engineId)
{
    m_engineId = engineId;
}

QMap<QString, QString> AnalyticsEngineEvent::details(common::SystemContext* context) const
{
    auto result = DescribedEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kPluginDetailName, engine(context));

    return result;
}

QString AnalyticsEngineEvent::engine(common::SystemContext* context) const
{
    using nx::analytics::taxonomy::AbstractState;
    using nx::analytics::taxonomy::AbstractEngine;

    if (m_engineId.isNull())
        return {};

    const std::shared_ptr<AbstractState> taxonomyState = context->analyticsTaxonomyState();
    if (!taxonomyState)
        return {};

    const AbstractEngine* engineInfo = taxonomyState->engineById(m_engineId.toString());
    if (engineInfo)
        return engineInfo->name();

    return {};
}

} // namespace nx::vms::rules
