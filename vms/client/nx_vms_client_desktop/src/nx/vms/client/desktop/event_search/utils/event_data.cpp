// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_data.h"

#include <QtGui/QPalette>

#include <core/resource/camera_resource.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/software_trigger_pixmaps.h>
#include <nx/vms/rules/utils/event_details.h>

namespace nx::vms::client::desktop {

using nx::vms::rules::Icon;

static const QMap<Icon, QString> kIconPaths = {
    {Icon::alert, "events/alert_20.svg"},
    {Icon::motion, "events/motion.svg"},
    {Icon::license, "events/license_20.svg"},
    {Icon::connection, "events/connection_20.svg"},
    {Icon::storage, "events/storage_20.svg"},
    {Icon::server, "events/server_20.svg"},
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

        case Icon::custom:
            return SoftwareTriggerPixmaps::effectivePixmapPath(custom);

        default:
            return kIconPaths.value(icon);
    }
}

QString eventTitle(const QVariantMap& details)
{
    auto detail = details.value(rules::utils::kAnalyticsEventTypeDetailName);

    if (!detail.isValid())
        detail = details.value(rules::utils::kNameDetailName);

    return detail.toString();
}

} // namespace nx::vms::client::desktop
