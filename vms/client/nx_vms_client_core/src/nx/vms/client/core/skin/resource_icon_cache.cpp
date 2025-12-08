// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_icon_cache.h"

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtWidgets/QApplication>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <network/system_helpers.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/cross_system/cross_system_camera_resource.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/skin/svg_icon_colorer.h>
#include <nx/vms/client/core/skin/svg_loader.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/system_settings.h>
#include <nx_ec/abstract_ec_connection.h>

using namespace nx::vms::client::core;

using nx::vms::client::core::CloudCrossSystemContext;
using nx::vms::client::core::LayoutResource;

namespace {

const SvgIconColorer::ThemeSubstitutions kTreeThemeSubstitutions = {
    {QnIcon::Disabled, {.primary = "light10", .secondary = "light4", .alpha=0.3}},
    {QnIcon::Selected, {.primary = "light4", .secondary = "light2"}},
    {QnIcon::Active, {.primary = "brand_core", .secondary= "light4"}},
    {QnIcon::Normal, {.primary = "light10", .secondary = "light4"}},
    {QnIcon::Error, {.primary = "red", .secondary = "red"}},
    {QnIcon::Pressed, {.primary = "light4", .secondary = "light2"}}};

const SvgIconColorer::ThemeSubstitutions kWarningTheme = {
    {QnIcon::Disabled, {.primary = "light10", .secondary = "yellow", .alpha=0.3}},
    {QnIcon::Selected, {.primary = "light4", .secondary = "yellow"}},
    {QnIcon::Active, {.primary = "brand_core", .secondary= "yellow"}},
    {QnIcon::Normal, {.primary = "light10", .secondary = "yellow"}},
    {QnIcon::Error, {.primary = "red", .secondary = "red"}},
    {QnIcon::Pressed, {.primary = "light4", .secondary = "yellow"}}};

const SvgIconColorer::ThemeSubstitutions kYellowTheme = {
    {QnIcon::Normal, {.primary = "yellow",}}};

const SvgIconColorer::ThemeSubstitutions kTreeThemeOfflineSubstitutions = {
    {QnIcon::Disabled, {.primary = "light10", .secondary = "red", .alpha = 0.3}},
    {QnIcon::Selected, {.primary = "light4", .secondary = "red", .alpha = 0.3}},
    {QnIcon::Active, {.primary = "brand_core", .secondary = "red", .alpha = 0.3}},
    {QnIcon::Normal, {.primary = "light10", .secondary = "red", .alpha = 0.3}},
    {QnIcon::Error, {.primary = "red", .secondary = "red", .alpha = 0.3}},
    {QnIcon::Pressed, {.primary = "light4", .secondary = "red", .alpha = 0.3}}};

NX_DECLARE_COLORIZED_ICON(kHasArchiveIcon, "20x20/Solid/archive.svg",\
    kEmptySubstitutions)
NX_DECLARE_COLORIZED_ICON(kRecordOnIcon, "20x20/Solid/record_on.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kRecordPartIcon, "20x20/Solid/record_part.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kCameraNotificationIcon, "20x20/Outline/device.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kCameraWarningIcon, "20x20/Solid/camera_warning.svg",\
    kWarningTheme)
NX_DECLARE_COLORIZED_ICON(kCameraOfflineIcon, "20x20/Solid/camera_offline.svg",\
    kTreeThemeOfflineSubstitutions)
NX_DECLARE_COLORIZED_ICON(kCameraUnauthorizedIcon, "20x20/Solid/camera_unauthorized.svg",\
    kTreeThemeOfflineSubstitutions)
NX_DECLARE_COLORIZED_ICON(kCameraIcon, "20x20/Solid/camera.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kCamerasIcon, "20x20/Solid/cameras.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kVirtualCamerasIcon, "20x20/Solid/virtual_camera.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kVirtualCamerasOfflineIcon, "20x20/Solid/virtual_camera.svg",\
    kTreeThemeOfflineSubstitutions)
NX_DECLARE_COLORIZED_ICON(kVirtualCamerasNotificationIcon, "20x20/Outline/virtual_camera.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kMultisensorIcon, "20x20/Solid/multisensor.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kMultisensorOfflineIcon, "20x20/Solid/multisensor_offline.svg",\
    kTreeThemeOfflineSubstitutions)
NX_DECLARE_COLORIZED_ICON(kMultisensorNotificationIcon, "20x20/Outline/multisensor.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kAlertYellowIcon, "20x20/Solid/alert2.svg", kYellowTheme)

NX_DECLARE_COLORIZED_ICON(kClientIcon, "20x20/Solid/client.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kSystemCloudUnauthIcon, "20x20/Solid/site_cloud_unauthorized.svg",\
    kTreeThemeOfflineSubstitutions)
NX_DECLARE_COLORIZED_ICON(kSystemCloudOfflineIcon, "20x20/Solid/site_cloud_offline.svg",\
    kTreeThemeOfflineSubstitutions)
NX_DECLARE_COLORIZED_ICON(kSystemCloudWarningIcon, "20x20/Solid/site_cloud_warning.svg",\
    kWarningTheme)
NX_DECLARE_COLORIZED_ICON(kSystemCloudIcon, "20x20/Solid/site_cloud.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kEncoderIcon, "20x20/Solid/encoder.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kEncoderNotificationIcon, "20x20/Outline/encoder.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kHealthMonitorsIcon, "20x20/Solid/health_monitors.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kHealthMonitorOfflineIcon, "20x20/Solid/health_monitor_offline.svg",\
    kTreeThemeOfflineSubstitutions)
NX_DECLARE_COLORIZED_ICON(kHealthMonitorIcon, "20x20/Solid/health_monitor.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kIntegrationProxiedIcon, "20x20/Solid/integration_proxied.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kIntegrationIcon, "20x20/Solid/integration.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kIntegrationsIcon, "20x20/Solid/integrations.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kIOOfflineIcon, "20x20/Solid/io_offline.svg",\
    kTreeThemeOfflineSubstitutions)
NX_DECLARE_COLORIZED_ICON(kIOWarningIcon, "20x20/Solid/io_warning.svg",\
    kTreeThemeOfflineSubstitutions)
NX_DECLARE_COLORIZED_ICON(kIOUnauthorizedIcon, "20x20/Solid/io_unauthorized.svg",\
    kTreeThemeOfflineSubstitutions)
NX_DECLARE_COLORIZED_ICON(kIOIcon, "20x20/Solid/io.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kIONotificationIcon, "20x20/Outline/io.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kLayoutCloudIcon, "20x20/Solid/layout_cloud.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kLayoutExportEncryptedIcon, "20x20/Solid/layout_exported_encrypted.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kLayoutSharedIcon, "20x20/Solid/layout_shared.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kLayoutTourIcon, "20x20/Solid/layout_tour.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kLayoutToursIcon, "20x20/Solid/layout_tours.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kLayoutIcon, "20x20/Solid/layout.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kLayoutsIcon, "20x20/Solid/layouts.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kLayoutsSharedIcon, "20x20/Solid/layouts_shared.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kLayoutsIntercomIcon, "20x20/Solid/layouts_intercom.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kFolderOpenIcon, "20x20/Solid/folder_open.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kMediaIcon, "20x20/Solid/media.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kMediaOfflineIcon, "20x20/Solid/media_offline.svg",\
    kTreeThemeOfflineSubstitutions)
NX_DECLARE_COLORIZED_ICON(kSnapshotIcon, "20x20/Solid/snapshot.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kSnapshotOfflineIcon, "20x20/Solid/snapshot_offline.svg",\
    kTreeThemeOfflineSubstitutions)

NX_DECLARE_COLORIZED_ICON(kMatrixIcon, "20x20/Solid/matrix.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kVideowallIcon, "20x20/Solid/videowall.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kScreenIcon, "20x20/Solid/screen.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kScreenLockedIcon, "20x20/Solid/screen_locked.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kScreenControlledIcon, "20x20/Solid/screen_controlled.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kScreenOfflineIcon, "20x20/Solid/screen_offline.svg",\
    kTreeThemeOfflineSubstitutions)

NX_DECLARE_COLORIZED_ICON(kSystemLocalIcon, "20x20/Solid/site_local.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kOtherSystemsIcon, "20x20/Solid/sites.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kServersIcon, "20x20/Solid/servers.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kServerIcon, "20x20/Solid/server.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kServerNotificationIcon, "20x20/Outline/server.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kServerOfflineIcon, "20x20/Solid/server_offline.svg",\
    kTreeThemeOfflineSubstitutions)
NX_DECLARE_COLORIZED_ICON(kServerCurrentIcon, "20x20/Solid/server_current.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kServerIncompatibleIcon, "20x20/Solid/server_incompatible.svg",\
    kWarningTheme)
NX_DECLARE_COLORIZED_ICON(kServerUnauthorizedIcon, "20x20/Solid/server_unauthorized.svg",\
    kTreeThemeOfflineSubstitutions)

NX_DECLARE_COLORIZED_ICON(kUserIcon, "20x20/Solid/user.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kUserNotificationIcon, "20x20/Outline/user.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kGroupIcon, "20x20/Solid/group.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kUserCloudIcon, "20x20/Solid/user_cloud.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kUserLdapIcon, "20x20/Solid/user_ldap.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kUserTempIcon, "20x20/Solid/user_temp.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kUserOrganizationIcon, "20x20/Solid/user_organization.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kUserChannelPartnerIcon, "20x20/Solid/user_cp.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kWebpagesIcon, "20x20/Solid/webpages.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kWebpageIcon, "20x20/Solid/webpage.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kWebpageProxiedIcon, "20x20/Solid/webpage_proxied.svg",\
    kTreeThemeSubstitutions)

bool isCurrentlyConnectedServer(const QnResourcePtr& resource)
{
    auto server = resource.dynamicCast<QnMediaServerResource>();
    if (!NX_ASSERT(server))
        return false;

    if (server->systemContext() != appContext()->currentSystemContext())
        return false;

    return !server->getId().isNull()
        && appContext()->currentSystemContext()->currentServerId() == server->getId();
}

using Key = ResourceIconCache::Key;
Key calculateStatus(Key key, const QnResourcePtr& resource)
{
    using Key = ResourceIconCache::KeyPart;
    using Status = ResourceIconCache::KeyPart;

    switch (resource->getStatus())
    {
        case nx::vms::api::ResourceStatus::recording:
        case nx::vms::api::ResourceStatus::online:
        {
            if (key == Key::Server && isCurrentlyConnectedServer(resource))
                return Status::Control;

            return Status::Online;
        }
        case nx::vms::api::ResourceStatus::offline:
            return Status::Offline;

        case nx::vms::api::ResourceStatus::unauthorized:
            return Status::Unauthorized;

        case nx::vms::api::ResourceStatus::incompatible:
            return Status::Incompatible;

        case nx::vms::api::ResourceStatus::mismatchedCertificate:
            return Status::MismatchedCertificate;

        default:
            break;
    };
    return Status::Unknown;
};

} //namespace

namespace nx::vms::client::core {

ResourceIconCache::IconWithDescription::IconWithDescription(
    const ColorizedIconDeclaration& colorizedIconDeclaration):
    iconDescription(colorizedIconDeclaration)
{
    icon = ResourceIconCache::loadIcon(
        colorizedIconDeclaration.iconPath(), colorizedIconDeclaration.themeSubstitutions());
}

ResourceIconCache::ResourceIconCache(QObject* parent):
    QObject(parent)
{
    // Systems.
    m_cache.insert(CurrentSystem, IconWithDescription(kSystemLocalIcon));
    m_cache.insert(OtherSystem, IconWithDescription(kSystemLocalIcon));
    m_cache.insert(OtherSystems, IconWithDescription(kOtherSystemsIcon));

    // Servers.
    m_cache.insert(Servers, IconWithDescription(kServersIcon));
    m_cache.insert(Server, IconWithDescription(kServerIcon));
    m_cache.insert(Server | Offline, IconWithDescription(kServerOfflineIcon));
    m_cache.insert(Server | Incompatible, IconWithDescription(kServerIncompatibleIcon));
    m_cache.insert(Server | Control, IconWithDescription(kServerCurrentIcon));
    m_cache.insert(Server | Unauthorized, IconWithDescription(kServerUnauthorizedIcon));
    // TODO: separate icon for Server with mismatched certificate.
    m_cache.insert(Server | MismatchedCertificate, IconWithDescription(kServerIncompatibleIcon));
    // Read-only server that is auto-discovered.
    m_cache.insert(Server | Incompatible | ReadOnly, IconWithDescription(kServerIncompatibleIcon));
    // Read-only server we are connected to.
    m_cache.insert(Server | Control | ReadOnly, IconWithDescription(kServerIncompatibleIcon));
    m_cache.insert(HealthMonitors, IconWithDescription(kHealthMonitorsIcon));
    m_cache.insert(HealthMonitor, IconWithDescription(kHealthMonitorIcon));
    m_cache.insert(HealthMonitor | Offline, IconWithDescription(kHealthMonitorOfflineIcon));

    // Layouts.
    m_cache.insert(Layouts, IconWithDescription(kLayoutsIcon));
    m_cache.insert(Layout, IconWithDescription(kLayoutIcon));
    m_cache.insert(ExportedLayout, IconWithDescription(kLayoutIcon));
    m_cache.insert(ExportedEncryptedLayout, IconWithDescription(kLayoutExportEncryptedIcon));
    m_cache.insert(IntercomLayout, IconWithDescription(kLayoutsIntercomIcon));
    m_cache.insert(SharedLayout, IconWithDescription(kLayoutSharedIcon));
    m_cache.insert(CloudLayout, IconWithDescription(kLayoutCloudIcon));
    m_cache.insert(SharedLayouts, IconWithDescription(kLayoutsSharedIcon));
    m_cache.insert(Showreel, IconWithDescription(kLayoutTourIcon));
    m_cache.insert(Showreels, IconWithDescription(kLayoutToursIcon));

    // Cameras.
    m_cache.insert(Cameras, IconWithDescription(kCamerasIcon));
    m_cache.insert(Camera, IconWithDescription(kCameraIcon));
    m_cache.insert(Camera | NotificationMode, IconWithDescription(kCameraNotificationIcon));
    m_cache.insert(Camera | Offline, IconWithDescription(kCameraOfflineIcon));
    m_cache.insert(Camera | Unauthorized, IconWithDescription(kCameraUnauthorizedIcon));
    m_cache.insert(Camera | Incompatible, IconWithDescription(kCameraWarningIcon));
    m_cache.insert(VirtualCamera, IconWithDescription(kVirtualCamerasIcon));
    m_cache.insert(VirtualCamera | Offline, IconWithDescription(kVirtualCamerasOfflineIcon));
    m_cache.insert(VirtualCamera | NotificationMode, IconWithDescription(kVirtualCamerasNotificationIcon));
    m_cache.insert(CrossSystemStatus | Unauthorized, IconWithDescription(kAlertYellowIcon));
    m_cache.insert(IOModule, IconWithDescription(kIOIcon));
    m_cache.insert(IOModule | NotificationMode, IconWithDescription(kIONotificationIcon));
    m_cache.insert(IOModule | Offline, IconWithDescription(kIOOfflineIcon));
    m_cache.insert(IOModule | Unauthorized, IconWithDescription(kIOUnauthorizedIcon));
    m_cache.insert(IOModule | Incompatible, IconWithDescription(kIOWarningIcon));
    m_cache.insert(Recorder, IconWithDescription(kEncoderIcon));
    m_cache.insert(Recorder | NotificationMode, IconWithDescription(kEncoderNotificationIcon));
    m_cache.insert(MultisensorCamera, IconWithDescription(kMultisensorIcon));
    m_cache.insert(MultisensorCamera | Offline, IconWithDescription(kMultisensorOfflineIcon));
    m_cache.insert(MultisensorCamera | NotificationMode, IconWithDescription(kMultisensorNotificationIcon));

    // Local files.
    m_cache.insert(LocalResources, IconWithDescription(kFolderOpenIcon));
    m_cache.insert(Image, IconWithDescription(kSnapshotIcon));
    m_cache.insert(Image | Offline, IconWithDescription(kSnapshotOfflineIcon));
    m_cache.insert(Media, IconWithDescription(kMediaIcon));
    m_cache.insert(Media | Offline, IconWithDescription(kMediaOfflineIcon));

    // Users.
    m_cache.insert(Users, IconWithDescription(kGroupIcon));
    m_cache.insert(User, IconWithDescription(kUserIcon));
    m_cache.insert(User | NotificationMode, IconWithDescription(kUserNotificationIcon));
    m_cache.insert(CloudUser, IconWithDescription(kUserCloudIcon));
    m_cache.insert(LdapUser, IconWithDescription(kUserLdapIcon));
    m_cache.insert(LocalUser, IconWithDescription(kUserIcon));
    m_cache.insert(TemporaryUser, IconWithDescription(kUserTempIcon));
    m_cache.insert(OrganizationUser, IconWithDescription(kUserOrganizationIcon));
    m_cache.insert(ChannelPartnerUser, IconWithDescription(kUserChannelPartnerIcon));

    // Videowalls.
    m_cache.insert(VideoWall, IconWithDescription(kVideowallIcon));
    m_cache.insert(VideoWallItem, IconWithDescription(kScreenIcon));
    m_cache.insert(VideoWallItem | Locked, IconWithDescription(kScreenLockedIcon));
    m_cache.insert(VideoWallItem | Control, IconWithDescription(kScreenControlledIcon));
    m_cache.insert(VideoWallItem | Offline, IconWithDescription(kScreenOfflineIcon));
    m_cache.insert(VideoWallMatrix, IconWithDescription(kMatrixIcon));

    // Integrations.
    m_cache.insert(Integrations, IconWithDescription(kIntegrationsIcon));
    m_cache.insert(Integration, IconWithDescription(kIntegrationIcon));
    m_cache.insert(IntegrationProxied, IconWithDescription(kIntegrationProxiedIcon));

    // Web Pages.
    m_cache.insert(WebPages, IconWithDescription(kWebpagesIcon));
    m_cache.insert(WebPage, IconWithDescription(kWebpageIcon));
    m_cache.insert(WebPageProxied, IconWithDescription(kWebpageProxiedIcon));

    // Analytics.
    m_cache.insert(AnalyticsEngine, IconWithDescription(kServerIcon));
    m_cache.insert(AnalyticsEngine | NotificationMode, IconWithDescription(kServerNotificationIcon));
    m_cache.insert(AnalyticsEngines, IconWithDescription(kServersIcon));
    m_cache.insert(AnalyticsEngine | Offline, IconWithDescription(kServerOfflineIcon));

    // Client.
    m_cache.insert(Client, IconWithDescription(kClientIcon));

    // Cloud system.
    m_cache.insert(CloudSystem, IconWithDescription(kSystemCloudIcon));
    m_cache.insert(CloudSystem | Offline, IconWithDescription(kSystemCloudOfflineIcon));
    m_cache.insert(CloudSystem | Locked, IconWithDescription(kSystemCloudWarningIcon));
    m_cache.insert(CloudSystem | Incompatible, IconWithDescription(kSystemCloudUnauthIcon));

    m_cache.insert(Unknown, {});
}

ResourceIconCache::~ResourceIconCache()
{
}

QColor ResourceIconCache::iconColor(const QnResourcePtr& resource) const
{
    return iconColor(key(resource));
}

QColor ResourceIconCache::iconColor(Key key, QnIcon::Mode mode) const
{
    auto it = m_cache.find(key);
    if (it == m_cache.end())
        return {}; //< Unexisting key

    const auto substitutions = it->iconDescription.themeSubstitutions();
    auto substitution = substitutions.find(mode);

    if (substitution == substitutions.end())
        return {}; //< Incorrect mode or icon without ColorizedIconDeclaration.

    return colorTheme()->color(substitution->primary);
}

QString ResourceIconCache::iconPath(Key key) const
{
    auto it = m_cache.find(key);
    if (it != m_cache.cend())
        return it->iconDescription.iconPath();

    it = m_cache.find(key & (AdditionalFlagsMask | StatusMask | TypeMask));
    if (it != m_cache.cend())
        return it->iconDescription.iconPath();

    it = m_cache.find(key & (AdditionalFlagsMask | TypeMask));
    if (it != m_cache.cend())
        return it->iconDescription.iconPath();

    it = m_cache.find(key & (TypeMask | StatusMask));
    if (it != m_cache.cend())
        return it->iconDescription.iconPath();

    it = m_cache.find(key & TypeMask);
    return NX_ASSERT(it != m_cache.cend()) ? it->iconDescription.iconPath() : QString{};
}

QString ResourceIconCache::iconPath(const QnResourcePtr& resource) const
{
    return iconPath(key(resource));
}

QPixmap ResourceIconCache::iconPixmap(Key key,
    const QSize& requestedSize,
    const SvgIconColorer::ThemeSubstitutions& svgColorSubstitutions,
    QnIcon::Mode mode)
{
    const auto& [icon, desc] = iconDescription(key);

    if (desc.iconPath().isEmpty()) //< Use preloaded legacy icons.
        return icon.pixmap(requestedSize, mode);

    QMap<QString /*source class name*/, QString /*target color name*/> customization;
    for (const auto& theme : {desc.themeSubstitutions(), svgColorSubstitutions})
    {
        if (auto it = theme.constFind(mode); it != theme.cend())
        {
            static const QString kPrimaryKey = "primary";
            static const QString kSecondaryKey = "secondary";
            static const QString kTertiaryKey = "tertiary";

            if (SvgIconColorer::ThemeColorsRemapData::isValidColor(it->primary))
                customization[kPrimaryKey] = it->primary;

            if (SvgIconColorer::ThemeColorsRemapData::isValidColor(it->secondary))
                customization[kSecondaryKey] = it->secondary;

            if (SvgIconColorer::ThemeColorsRemapData::isValidColor(it->tertiary))
                customization[kTertiaryKey] = it->tertiary;
        }
    }

    const auto& path = qnSkin->path(desc.iconPath());
    return loadSvgImage(path, customization, requestedSize, qApp->devicePixelRatio());
}

ResourceIconCache::IconWithDescription ResourceIconCache::searchIconOutsideCache(Key key)
{
    IconWithDescription data;

    if (key & AlwaysSelected)
    {
        QIcon resultIcon;
        auto sourceData = iconDescription(key & ~AlwaysSelected);
        for (const QSize& size : sourceData.icon.availableSizes(QIcon::Selected))
        {
            QPixmap selectedPixmap = sourceData.icon.pixmap(size, QIcon::Selected);
            resultIcon.addPixmap(selectedPixmap, QIcon::Normal);
            resultIcon.addPixmap(selectedPixmap, QIcon::Selected);
        }
        data.icon = resultIcon;
        data.iconDescription = sourceData.iconDescription;
    }
    else
    {
        const auto base = key & TypeMask;
        const auto status = key & StatusMask;

        if (!NX_ASSERT(status == ResourceIconCache::Online))
            return {};

        data = m_cache.value(base);
    }

    m_cache.insert(key, data);
    return data;
}

ResourceIconCache::IconWithDescription ResourceIconCache::iconDescription(Key key)
{
    if ((key & TypeMask) == Unknown)
        key = Unknown;

    if (m_cache.contains(key))
        return m_cache.value(key);

    return searchIconOutsideCache(key);
}

QIcon ResourceIconCache::icon(Key key)
{
    /* This function will be called from GUI thread only,
     * so no synchronization is needed. */
    return iconDescription(key).icon;
}

QIcon ResourceIconCache::icon(const QnResourcePtr& resource)
{
    if (!resource)
        return QIcon();
    return icon(key(resource));
}

ResourceIconCache::Key ResourceIconCache::userKey(const QnUserResourcePtr& user) const
{
    if (!NX_ASSERT(user))
        return User;

    if (user->isOrg())
        return OrganizationUser;

    if (user->isChannelPartner())
        return ChannelPartnerUser;

    if (user->isCloud())
        return CloudUser;

    if (user->isLdap())
        return LdapUser;

    if (user->isTemporary())
        return TemporaryUser;

    if (user->isLocal())
        return LocalUser;

    return User;
}

ResourceIconCache::Key ResourceIconCache::key(const QnResourcePtr& resource) const
{
    Key key = Unknown;

    Qn::ResourceFlags flags = resource->flags();
    if (flags.testFlag(Qn::local_server))
    {
        key = LocalResources;
    }
    else if (flags.testFlag(Qn::server))
    {
        key = Server;
    }
    else if (flags.testFlag(Qn::io_module))
    {
        key = IOModule;
    }
    else if (flags.testFlag(Qn::virtual_camera))
    {
        key = VirtualCamera;
    }
    else if (flags.testFlag(Qn::live_cam))
    {
        key = Camera;
    }
    else if (flags.testFlag(Qn::local_image))
    {
        key = Image;
    }
    else if (flags.testFlag(Qn::local_video))
    {
        key = Media;
    }
    else if (flags.testFlag(Qn::user))
    {
        key = userKey(resource.dynamicCast<QnUserResource>());
    }
    else if (flags.testFlag(Qn::videowall))
    {
        key = VideoWall;
    }
    else if (const auto engine = resource.dynamicCast<nx::vms::common::AnalyticsEngineResource>())
    {
        key = AnalyticsEngine;
    }
    else if (flags.testFlag(Qn::web_page))
    {
        const auto webPage = resource.dynamicCast<QnWebPageResource>();
        if (!NX_ASSERT(webPage))
            return Key(WebPage);

        QnWebPageResource::Options options = webPage->getOptions();
        switch (options)
        {
            case QnWebPageResource::WebPage: return Key(WebPage);
            case QnWebPageResource::ProxiedWebPage: return Key(WebPageProxied);
            case QnWebPageResource::Integration: return Key(Integration);
            case QnWebPageResource::ProxiedIntegration: return Key(IntegrationProxied);
        }

        NX_ASSERT(false, "Unexpected value (%1)", options);
        return Key(WebPage);
    }

    Key status = calculateStatus(key, resource);

    if (auto csCamera = resource.dynamicCast<CrossSystemCameraResource>();
        csCamera && flags.testFlag(Qn::fake))
    {
        const auto systemId = csCamera->systemId();
        const auto context = appContext()->cloudCrossSystemManager()->systemContext(systemId);
        if (NX_ASSERT(context))
        {
            const auto systemStatus = context->status();
            if (systemStatus == CloudCrossSystemContext::Status::connecting)
            {
                key = CrossSystemStatus;
                status = ResourceIconCache::Control;
            }
            else if (systemStatus == CloudCrossSystemContext::Status::connectionFailure
                && context->systemDescription()->isOnline())
            {
                status = ResourceIconCache::Unauthorized;
            }
        }
    }
    else if (const auto layout = resource.dynamicCast<LayoutResource>())
    {
        if (layout->layoutType() == LayoutResource::LayoutType::intercom)
            key = IntercomLayout;
        else if (layout->isCrossSystem())
            key = CloudLayout;
        else if (layout->isShared())
            key = SharedLayout;
        else if (layout->isFile())
            key = ExportedLayout;
        else
            key = Layout;

        status = Unknown;
    }
    else if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        const auto isBuggy =
            getResourceExtraStatus(camera).testFlag(ResourceExtraStatusFlag::buggy);
        if (status == Online && (camera->needsToChangeDefaultPassword() || isBuggy))
            status = Incompatible;
    }

    if (flags.testFlag(Qn::read_only))
        status |= ReadOnly;

    return Key(key | status);
}

QIcon ResourceIconCache::cameraRecordingStatusIcon(ResourceExtraStatus status) const
{
    if (status.testFlag(ResourceExtraStatusFlag::recording))
        return qnSkin->icon(kRecordOnIcon);

    if (status.testFlag(ResourceExtraStatusFlag::scheduled))
        return qnSkin->icon(kRecordPartIcon);

    if (status.testFlag(ResourceExtraStatusFlag::hasArchive))
        return qnSkin->icon(kHasArchiveIcon);

    return QIcon();
}

QIcon ResourceIconCache::loadIcon(const QString& name,
    const SvgIconColorer::ThemeSubstitutions& themeSubstitutions)
{
    static const QnIcon::Suffixes kResourceIconSuffixes({
        {QnIcon::Active,   "accented"},
        {QnIcon::Disabled, "disabled"},
        {QnIcon::Selected, "selected"},
        });

    return qnSkin->icon(name,
        /* checkedName */ QString(),
        &kResourceIconSuffixes,
        name.endsWith(".svg")
            ? SvgIconColorer::kTreeIconSubstitutions
            : SvgIconColorer::kDefaultIconSubstitutions,
        /* svgCheckedColorSubstitutions */ {},
        themeSubstitutions);
}

SvgIconColorer::ThemeSubstitutions ResourceIconCache::treeThemeSubstitutions()
{
    return kTreeThemeSubstitutions;
}

SvgIconColorer::ThemeSubstitutions ResourceIconCache::treeThemeOfflineSubstitutions()
{
    return kTreeThemeOfflineSubstitutions;
}

} // namespace nx::vms::client::core
