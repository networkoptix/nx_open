// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_data.h"

#include <QtGui/QPalette>

#include <core/resource/camera_resource.h>
#include <nx/vms/client/core/analytics/analytics_icon_manager.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/software_trigger_pixmaps.h>
#include <nx/vms/rules/utils/event_details.h>

namespace nx::vms::client::desktop {

using nx::vms::rules::Icon;

static const QMap<Icon, QString> kIconPaths = {
    {Icon::alert, "20x20/Outline/image.svg"},
    {Icon::motion, "20x20/Outline/motion.svg"},
    {Icon::license, "20x20/Outline/key.svg"},
    {Icon::networkIssue, "20x20/Outline/network.svg"},
    {Icon::storage, "20x20/Outline/storage.svg"},
    {Icon::server, "20x20/Outline/server.svg"},
    {Icon::inputSignal, "20x20/Outline/input_signal.svg"},
    {Icon::pluginDiagnostic, "20x20/Outline/plugin.svg"},
    {Icon::fanError, "20x20/Outline/fan.svg"},
     // TODO: VMS-47520: check how this event is called and create if required.
    {Icon::cloudOffline, "20x20/Outline/cloud_offline.svg"},
    {Icon::analyticsEvent, "20x20/Outline/analytics.svg"},
    {Icon::generic, "20x20/Outline/generic.svg"},
};

bool needIconDevices(nx::vms::rules::Icon icon)
{
    return icon == nx::vms::rules::Icon::resource;
}

QString eventIconPath(
    nx::vms::rules::Icon icon,
    const QString& custom,
    const QnResourceList& devices)
{
    switch (icon)
    {
        case Icon::none:
            return {};

        case Icon::resource:
        {
            return devices.isEmpty()
                ? qnResIconCache->iconPath(
                      QnResourceIconCache::Camera | QnResourceIconCache::NotificationMode)
                : qnResIconCache->iconPath(QnResourceIconCache::key(devices.front())
                      | QnResourceIconCache::NotificationMode);
        }

        case Icon::softTrigger:
            return SoftwareTriggerPixmaps::effectivePixmapPath(custom);

        case Icon::analyticsObjectDetected:
            return core::analytics::IconManager::instance()->iconPath(custom);

        default:
            return kIconPaths.value(icon);
    }
}

QString eventTitle(const QVariantMap& details)
{
    auto detail = details.value(rules::utils::kAnalyticsEventTypeDetailName);

    if(!detail.isValid())
        detail = details.value(rules::utils::kCaptionDetailName);

    if (!detail.isValid())
        detail = details.value(rules::utils::kNameDetailName);

    return detail.toString();
}

} // namespace nx::vms::client::desktop
