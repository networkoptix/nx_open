// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QVariantMap>

namespace nx::vms::rules::utils {

static constexpr auto kAnalyticsEventTypeDetailName = "analyticsEventType";
static constexpr auto kAnalyticsObjectTypeDetailName = "analyticsObjectType";
static constexpr auto kCaptionDetailName = "caption";
static constexpr auto kCountDetailName = "count";
static constexpr auto kDescriptionDetailName = "description";
static constexpr auto kDetailingDetailName = "detailing";
static constexpr auto kExtendedCaptionDetailName = "extendedCaption";
static constexpr auto kEmailTemplatePathDetailName = "extendedCaption";
static constexpr auto kHasScreenshotDetailName = "hasTimestamp";
static constexpr auto kNameDetailName = "name";
static constexpr auto kPluginIdDetailName = "pluginId";
static constexpr auto kReasonDetailName = "reason";
static constexpr auto kScreenshotLifetimeDetailName = "screenshotLifetime";
static constexpr auto kSourceIdDetailName = "sourceId";
static constexpr auto kSourceNameDetailName = "sourceName";
static constexpr auto kSourceStatusDetailName = "sourceStatus";
static constexpr auto kTypeDetailName = "type";
static constexpr auto kTriggerNameDetailName = "triggerName";
static constexpr auto kUserIdDetailName = "userId";

static constexpr std::chrono::microseconds kScreenshotLifetime(5'000'000);

template <class T>
void insertIfNotEmpty(QVariantMap& map, const QString& key, const T& value)
{
    if (!value.isEmpty())
        map.insert(key, value);
}

inline void insertIfValid(QVariantMap& map, const QString& key, const QVariant& value)
{
    if (value.isValid())
        map.insert(key, value);
}

} // namespace nx::vms::rules::utils
