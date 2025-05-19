// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QVariantMap>

#include <nx/vms/event/level.h>
#include <nx/vms/rules/client_action.h>
#include <nx/vms/rules/icon.h>

namespace nx::vms::rules::utils {

/**
 * Aggregation key string representation. For the most of the events this would be either server or
 * camera name.
 */
static constexpr auto kAggregationKeyDetailName = "aggregationKey";

/**
 * Aggregation key details. For the most of the events this would be either server or camera IP
 * address. Needs to be split from the `aggregationKey` due to formatting reasons.
 */
static constexpr auto kAggregationKeyDetailsDetailName = "aggregationKeyDetails";

/**
 * Aggregation icon id. For the most of the events this would be either 'server' or 'camera'.
 */
static constexpr auto kAggregationKeyIconDetailName = "aggregationKeyIcon";

static constexpr auto kAnalyticsEventTypeDetailName = "analyticsEventType";

/**
 * Caption of the event. Will be displayed on a tile for the NotificationAction and as the title
 * text for the PushNotificationAction. In case of aggregated event only the first event is used.
 * Only minimal required details set is included. Some event types use user-provided caption.
 * Example: "Input Port: Port_1".
 */
static constexpr auto kCaptionDetailName = "caption";

static constexpr auto kClientActionDetailName = "clientAction";
static constexpr auto kCustomIconDetailName = "customIcon";

/**
 * Description of the event. Will be displayed on a tile for the NotificationAction and as a full
 * text for the PushNotificationAction. In case of aggregated event only the first event is used.
 * Only minimal required details set is included. Some event types use user-provided description.
 * Example: "Input Port: Port_1".
 */
static constexpr auto kDescriptionDetailName = "description";

/** Url or email destination. Actual for the HttpAction and SendEmailAction only. */
static constexpr auto kDestinationDetailName = "destination";

/**
 * Extended event caption with resource name. Used as email subject or bookmark name.
 * Example: "Camera disconnected at Server1".
 */
static constexpr auto kExtendedCaptionDetailName = "extendedCaption";

/**
 * Verbose event details. Used as bookmark description, as text overlay text and as a tooltip in
 * the desktop notification tiles. Also displayed in the Event Log.
 */
static constexpr auto kDetailingDetailName = "detailing";

/** Verbose event details in the html format. Used in the SendEmail action only. */
static constexpr auto kHtmlDetailsName = "htmlDetails";

static constexpr auto kIconDetailName = "icon";
static constexpr auto kLevelDetailName = "level";

/**
 * List of source resources ids for the event. Used in "Source" column of the Event Log to show
 * correct context menu or to be used in filters. By default this is the id of the device (if
 * present) or server where the event was produced. Some events may overwrite it.
 */
static constexpr auto kSourceResourcesIdsDetailName = "sourceResourcesIds";

/**
 * Type of source resources for the event. Used in "Source" column of the Event Log to show the
 * correct icon. Value of ResourceType type.
 */
static constexpr auto kSourceResourcesTypeDetailName = "sourceResourcesType";

/**
 * Source text for the event. Used in the {event.source} substitution and in the "Source" column of
 * the Event Log. By default this is the device (if present) or server name where the event was
 * produced. Some events may overwrite it.
 */
static constexpr auto kSourceTextDetailName = "sourceText";

/**
 * Target url for the `browseUrl` client action. Used in the Device IP Conflict event only.
 */
static constexpr auto kUrlDetailName = "url";

/**
 * ID of the user that triggered the event. Applicable to Soft Trigger only. Used only in the
 * {user.name} field substitution.
 */
static constexpr auto kUserIdDetailName = "userId";

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

inline void insertLevel(QVariantMap& map, nx::vms::event::Level level)
{
    map.insert(kLevelDetailName, QVariant::fromValue(level));
}

inline void insertIcon(QVariantMap& map, nx::vms::rules::Icon icon)
{
    map.insert(kIconDetailName, QVariant::fromValue(icon));
}

inline void insertClientAction(QVariantMap& map, nx::vms::rules::ClientAction action)
{
    map.insert(kClientActionDetailName, QVariant::fromValue(action));
}

} // namespace nx::vms::rules::utils
