// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_engine_event.h"

#include "../utils/event_details.h"
#include "../utils/type.h"

namespace nx::vms::rules {

AnalyticsEngineEvent::AnalyticsEngineEvent(
    std::chrono::microseconds timestamp,
    const QString& caption,
    const QString& description,
    nx::Uuid cameraId,
    nx::Uuid engineId)
    :
    DescribedEvent(timestamp, caption, description),
    m_cameraId(cameraId),
    m_engineId(engineId)
{
}

nx::Uuid AnalyticsEngineEvent::cameraId() const
{
    return m_cameraId;
}

void AnalyticsEngineEvent::setCameraId(nx::Uuid cameraId)
{
    m_cameraId = cameraId;
}

nx::Uuid AnalyticsEngineEvent::engineId() const
{
    return m_engineId;
}

void AnalyticsEngineEvent::setEngineId(nx::Uuid engineId)
{
    m_engineId = engineId;
}

QString AnalyticsEngineEvent::resourceKey() const
{
    return m_cameraId.toSimpleString();
}

QVariantMap AnalyticsEngineEvent::details(common::SystemContext* context) const
{
    auto result = DescribedEvent::details(context);

    utils::insertIfValid(result, utils::kPluginIdDetailName, QVariant::fromValue(m_engineId));
    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, detailing());

    return result;
}

QString AnalyticsEngineEvent::detailing() const
{
    QString eventDetailing = caption();

    if (!description().isEmpty())
        eventDetailing += eventDetailing.isEmpty() ? description() : ": " + description();

    return eventDetailing;
}

} // namespace nx::vms::rules
