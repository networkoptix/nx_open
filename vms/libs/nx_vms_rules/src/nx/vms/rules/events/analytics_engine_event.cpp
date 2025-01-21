// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_engine_event.h"

#include "../utils/event_details.h"
#include "../utils/type.h"

namespace nx::vms::rules {

AnalyticsEngineEvent::AnalyticsEngineEvent(
    std::chrono::microseconds timestamp,
    const QString& caption,
    const QString& description,
    nx::Uuid deviceId,
    nx::Uuid engineId)
    :
    DescribedEvent(timestamp, caption, description),
    m_deviceId(deviceId),
    m_engineId(engineId)
{
}

nx::Uuid AnalyticsEngineEvent::deviceId() const
{
    return m_deviceId;
}

void AnalyticsEngineEvent::setDeviceId(nx::Uuid deviceId)
{
    m_deviceId = deviceId;
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
    return deviceId().toSimpleString();
}

QVariantMap AnalyticsEngineEvent::details(
    common::SystemContext* context, const nx::vms::api::rules::PropertyMap& aggregatedInfo) const
{
    auto result = DescribedEvent::details(context, aggregatedInfo);
    utils::insertIfValid(result, utils::kPluginIdDetailName, QVariant::fromValue(m_engineId));
    result.insert(utils::kDetailingDetailName, detailing());

    return result;
}

QStringList AnalyticsEngineEvent::detailing() const
{
    QString eventDetailing = caption();

    if (!description().isEmpty())
        eventDetailing += eventDetailing.isEmpty() ? description() : ": " + description();

    return QStringList{eventDetailing};
}

} // namespace nx::vms::rules
