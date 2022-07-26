// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_engine_event.h"

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

QString AnalyticsEngineEvent::uniqueName() const
{
    return makeName(DescribedEvent::uniqueName(), m_cameraId.toSimpleString());
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
