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
    {Icon::alert, "16x16/Outline/image.svg"},
    {Icon::motion, "16x16/Outline/motion.svg"},
    {Icon::license, "16x16/Outline/key.svg"},
    {Icon::networkIssue, "16x16/Outline/network.svg"},
    {Icon::storage, "16x16/Outline/storage.svg"},
    {Icon::server, "16x16/Outline/server.svg"},
    {Icon::inputSignal, "16x16/Outline/input_signal.svg"},
    {Icon::pluginDiagnostic, "16x16/Outline/plugin.svg"},
    {Icon::fanError, "16x16/Outline/fan.svg"},
     // TODO: VMS-47520: check how this event is called and create if required.
    {Icon::cloudOffline, "16x16/Outline/cloud_offline.svg"},
    {Icon::analyticsEvent, "16x16/Outline/analytics.svg"},
    {Icon::generic, "16x16/Outline/generic.svg"},
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
            return devices.isEmpty()
                ? qnResIconCache->iconPath(QnResourceIconCache::Camera)
                : qnResIconCache->iconPath(devices.front());

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
