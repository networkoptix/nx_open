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

using nx::vms::event::Level;
using nx::vms::rules::Icon;

namespace {

static const QColor kLight12Color = "#91A7B2";
static const QColor kLight10Color = "#A5B7C0";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QnIcon::Normal, {{kLight12Color, "light12"}, {kLight10Color, "light10"}}},
};

struct IconInfo
{
    QString defaultPath;
    QMap<Level, QString> pathByLevel;

    QString icon(Level level) const
    {
        return pathByLevel.value(level, defaultPath);
    }
};

static const QMap<Icon, IconInfo> kIconByLevel = {
    {Icon::alert, {"events/alert_20.svg", {
        {Level::critical, "events/alert_red.png"},
        {Level::important, "events/alert_yellow.png"},
    }}},
    {Icon::motion, {"events/motion.svg", {}}},
    {Icon::license, {"events/license_red.png", {}}},
    {Icon::connection, {"events/connection_20.svg", {
        {Level::critical, "events/connection_red.png"},
        {Level::important, "events/connection_yellow.png"},
    }}},
    {Icon::storage, {"events/storage_20.svg", {
        {Level::success, "events/storage_green.png"},
        {Level::critical, "events/storage_red.png"},
    }}},
    {Icon::server, {"events/server_20.svg", {
        {Level::critical, "events/server_red.png"},
        {Level::important, "events/server_yellow.png"},
    }}},

};

QPixmap toPixmap(const QIcon& icon)
{
    return core::Skin::maximumSizePixmap(icon);
}

} // namespace

bool needIconDevices(nx::vms::rules::Icon icon)
{
    return icon == nx::vms::rules::Icon::resource;
}

QPixmap eventIcon(
    nx::vms::rules::Icon icon,
    nx::vms::event::Level level,
    const QString& custom,
    const QColor& color,
    const QnResourceList& devices)
{
    switch (icon)
    {
        case Icon::none:
            return {};

        case Icon::resource:
            return toPixmap(!devices.isEmpty()
                ? qnResIconCache->icon(devices.front())
                : qnResIconCache->icon(QnResourceIconCache::Camera));

        case Icon::custom:
            return SoftwareTriggerPixmaps::colorizedPixmap(
                custom,
                color.isValid() ? color : QPalette().light().color());

        default:
            if (const auto& path = kIconByLevel.value(icon).icon(level);
                NX_ASSERT(!path.isEmpty(), "Unhandled icon: %1, level: %2, devices: %3",
                    icon, level, devices))
            {
                if (path.endsWith(".svg"))
                    return qnSkin->icon(path, kIconSubstitutions).pixmap(QSize(20, 20));

                return qnSkin->colorize(qnSkin->pixmap(path), color);
            }

            return {};
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
