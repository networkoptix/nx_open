// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_icon_cache.h"

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <network/base_system_description.h>
#include <network/system_helpers.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/skin/svg_icon_colorer.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/cross_system/cross_system_camera_resource.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/server.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/system_settings.h>
#include <nx_ec/abstract_ec_connection.h>

using namespace nx::vms::client::desktop;

Q_GLOBAL_STATIC(QnResourceIconCache, qn_resourceIconCache);

namespace {
static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kTreeThemeSubstitutions = {
    {QnIcon::Disabled, {.primary = "light10", .secondary = "light4", .alpha=0.3}},
    {QnIcon::Selected, {.primary = "light4", .secondary = "light2"}},
    {QnIcon::Active, {.primary = "brand_core", .secondary= "light4"}},
    {QnIcon::Normal, {.primary = "light10", .secondary = "light4"}},
    {QnIcon::Error, {.primary = "red_l2", .secondary = "red_l2"}},
    {QnIcon::Pressed, {.primary = "light4", .secondary = "light2"}}};

NX_DECLARE_COLORIZED_ICON(kHasArchiveIcon, "20x20/Solid/archive.svg",\
    nx::vms::client::core::kEmptySubstitutions)
NX_DECLARE_COLORIZED_ICON(kRecordOnIcon, "20x20/Solid/record_on.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kRecordPartIcon, "20x20/Solid/record_part.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kCameraWarningIcon, "20x20/Solid/camera_warning.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kCameraOfflineIcon, "20x20/Solid/camera_offline.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kCameraUnauthorizedIcon, "20x20/Solid/camera_unauthorized.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kCameraIcon, "20x20/Solid/camera.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kCamerasIcon, "20x20/Solid/cameras.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kVirtualCamerasIcon, "20x20/Solid/virtual_camera.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kMultisensorIcon, "20x20/Solid/multisensor.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kClientIcon, "20x20/Solid/client.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kSystemCloudUnauthIcon, "20x20/Solid/system_cloud_unauthorized.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kSystemCloudOfflineIcon, "20x20/Solid/system_cloud_offline.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kSystemCloudWarningIcon, "20x20/Solid/system_cloud_warning.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kSystemCloudIcon, "20x20/Solid/system_cloud.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kEncoderIcon, "20x20/Solid/encoder.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kHealthMonitorOfflineIcon, "20x20/Solid/health_monitor_offline.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kHealthMonitorIcon, "20x20/Solid/health_monitor.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kExtensionProxiedIcon, "20x20/Solid/extension_proxied.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kExtensionIcon, "20x20/Solid/extension.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kExtensionsIcon, "20x20/Solid/extensions.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kIOOfflineIcon, "20x20/Solid/io_offline.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kIOUnauthorizedIcon, "20x20/Solid/io_unauthorized.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kIOIcon, "20x20/Solid/io.svg",\
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

NX_DECLARE_COLORIZED_ICON(kFolderOpenIcon, "20x20/Solid/ws_folder_open.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kMediaIcon, "20x20/Solid/media.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kMediaOfflineIcon, "20x20/Solid/media_offline.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kSnapshotIcon, "20x20/Solid/snapshot.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kSnapshotOfflineIcon, "20x20/Solid/snapshot_offline.svg",\
    kTreeThemeSubstitutions)

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
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kSystemLocalIcon, "20x20/Solid/system_local.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kOtherSystemsIcon, "20x20/Solid/other_systems.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kServersIcon, "20x20/Solid/servers.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kServerIcon, "20x20/Solid/server.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kServerOfflineIcon, "20x20/Solid/server_offline.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kServerCurrentIcon, "20x20/Solid/server_current.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kServerIncompatibleIcon, "20x20/Solid/server_incompatible.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kServerUnauthorizedIcon, "20x20/Solid/server_unauthorized.svg",\
    kTreeThemeSubstitutions)

NX_DECLARE_COLORIZED_ICON(kUserIcon, "20x20/Solid/user.svg",\
    kTreeThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kGroupIcon, "20x20/Solid/group.svg",\
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

bool isCompatibleServer(const QnResourcePtr& resource)
{
    auto server = resource.dynamicCast<ServerResource>();
    return NX_ASSERT(server) && server->isCompatible();
}

bool isDetachedServer(const QnResourcePtr& resource)
{
    auto server = resource.dynamicCast<ServerResource>();
    return NX_ASSERT(server) && server->isDetached();
}

QIcon loadIcon(const QString& name,
    const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions& themeSubstitutions = {})
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
            ? nx::vms::client::core::SvgIconColorer::kTreeIconSubstitutions
            : nx::vms::client::core::SvgIconColorer::kDefaultIconSubstitutions,
        /* svgCheckedColorSubstitutions */ {},
        themeSubstitutions);
}

using Key = QnResourceIconCache::Key;
Key calculateStatus(Key key, const QnResourcePtr& resource)
{
    using Key = QnResourceIconCache::KeyPart;
    using Status = QnResourceIconCache::KeyPart;

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
        {
            if (key == Key::Server && !isCompatibleServer(resource) && !isDetachedServer(resource))
                return Status::Incompatible;

            return Status::Offline;
        }

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

QnResourceIconCache::QnResourceIconCache(QObject* parent):
    QObject(parent)
{
    m_cache.insert(Unknown, QIcon());

    // Systems.
    m_cache.insert(CurrentSystem, qnSkin->icon(kSystemLocalIcon));
    // TODO: pprivalov ask dbatov how it should looks actually
    m_cache.insert(OtherSystem, qnSkin->icon(kSystemLocalIcon));
    m_cache.insert(OtherSystems, qnSkin->icon(kOtherSystemsIcon));

    // Servers.
    m_cache.insert(Servers, qnSkin->icon(kServersIcon));
    m_cache.insert(Server, qnSkin->icon(kServerIcon));
    m_cache.insert(Server | Offline, qnSkin->icon(kServerOfflineIcon));
    m_cache.insert(Server | Incompatible, qnSkin->icon(kServerIncompatibleIcon));
    m_cache.insert(Server | Control, qnSkin->icon(kServerCurrentIcon));
    m_cache.insert(Server | Unauthorized, qnSkin->icon(kServerUnauthorizedIcon));
    // TODO: separate icon for Server with mismatched certificate.
    // TODO: pprivalov check that it works correctly with svg and if previous todo is still valid
    m_cache.insert(Server | MismatchedCertificate, qnSkin->icon(kServerIncompatibleIcon));
    // Read-only server that is auto-discovered.
    // TODO: pprivalov check that correct collor is used
    m_cache.insert(Server | Incompatible | ReadOnly,
        qnSkin->icon(kServerIncompatibleIcon));
    // Read-only server we are connected to.
    m_cache.insert(Server | Control | ReadOnly, qnSkin->icon(kServerIncompatibleIcon));
    m_cache.insert(HealthMonitor, qnSkin->icon(kHealthMonitorIcon));
    m_cache.insert(HealthMonitor | Offline, qnSkin->icon(kHealthMonitorOfflineIcon));

    // Layouts.
    m_cache.insert(Layouts, qnSkin->icon(kLayoutsIcon));
    m_cache.insert(Layout, qnSkin->icon(kLayoutIcon));
    m_cache.insert(ExportedLayout, qnSkin->icon(kLayoutIcon));
    m_cache.insert(ExportedEncryptedLayout, qnSkin->icon(kLayoutExportEncryptedIcon));
    m_cache.insert(IntercomLayout, qnSkin->icon(kLayoutsIntercomIcon));
    m_cache.insert(SharedLayout, qnSkin->icon(kLayoutSharedIcon));
    m_cache.insert(CloudLayout, qnSkin->icon(kLayoutCloudIcon));
    m_cache.insert(SharedLayouts, qnSkin->icon(kLayoutsSharedIcon));
    m_cache.insert(Showreel, qnSkin->icon(kLayoutTourIcon));
    m_cache.insert(Showreels, qnSkin->icon(kLayoutToursIcon));

    // Cameras.
    m_cache.insert(Cameras, qnSkin->icon(kCamerasIcon));
    m_cache.insert(Camera, qnSkin->icon(kCameraIcon));
    m_cache.insert(Camera | Offline, qnSkin->icon(kCameraOfflineIcon));
    m_cache.insert(Camera | Unauthorized, qnSkin->icon(kCameraUnauthorizedIcon));
    m_cache.insert(Camera | Incompatible, qnSkin->icon(kCameraWarningIcon));
    m_cache.insert(VirtualCamera, qnSkin->icon(kVirtualCamerasIcon));
    m_cache.insert(CrossSystemStatus | Unauthorized, loadIcon("events/alert_yellow.png"));
    m_cache.insert(CrossSystemStatus | Control, loadIcon("legacy/loading.gif")); //< The Control uses to describe loading state.
    m_cache.insert(CrossSystemStatus | Offline, loadIcon("cloud/cloud_20_disabled.png"));
    m_cache.insert(IOModule, qnSkin->icon(kIOIcon));
    m_cache.insert(IOModule | Offline, qnSkin->icon(kIOOfflineIcon));
    m_cache.insert(IOModule | Unauthorized, qnSkin->icon(kIOUnauthorizedIcon));
    m_cache.insert(IOModule | Incompatible, qnSkin->icon(kCameraWarningIcon));
    m_cache.insert(Recorder, qnSkin->icon(kEncoderIcon));
    m_cache.insert(MultisensorCamera, qnSkin->icon(kMultisensorIcon));

    // Local files.
    m_cache.insert(LocalResources, qnSkin->icon(kFolderOpenIcon));
    m_cache.insert(Image, qnSkin->icon(kSnapshotIcon));
    m_cache.insert(Image | Offline, qnSkin->icon(kSnapshotOfflineIcon));
    m_cache.insert(Media, qnSkin->icon(kMediaIcon));
    m_cache.insert(Media | Offline, qnSkin->icon(kMediaOfflineIcon));

    // Users.
    m_cache.insert(Users, qnSkin->icon(kGroupIcon));
    m_cache.insert(User, qnSkin->icon(kUserIcon));
    m_cache.insert(CloudUser, loadIcon("user_settings/user_cloud.svg", kTreeThemeSubstitutions));
    m_cache.insert(LdapUser, loadIcon("user_settings/user_ldap.svg", kTreeThemeSubstitutions));
    m_cache.insert(LocalUser, loadIcon("user_settings/user_local.svg", kTreeThemeSubstitutions));
    m_cache.insert(
        TemporaryUser, loadIcon("user_settings/user_local_temp.svg", kTreeThemeSubstitutions));

    // Videowalls.
    m_cache.insert(VideoWall, qnSkin->icon(kVideowallIcon));
    m_cache.insert(VideoWallItem, qnSkin->icon(kScreenIcon));
    m_cache.insert(VideoWallItem | Locked, qnSkin->icon(kScreenLockedIcon));
    m_cache.insert(VideoWallItem | Control, qnSkin->icon(kScreenControlledIcon));
    m_cache.insert(VideoWallItem | Offline, qnSkin->icon(kScreenOfflineIcon));
    m_cache.insert(VideoWallMatrix, qnSkin->icon(kMatrixIcon));

    // Integrations.
    m_cache.insert(Integrations, qnSkin->icon(kExtensionsIcon));
    m_cache.insert(Integration, qnSkin->icon(kExtensionIcon));
    m_cache.insert(IntegrationProxied, qnSkin->icon(kExtensionProxiedIcon));

    // Web Pages.
    m_cache.insert(WebPages, qnSkin->icon(kWebpagesIcon));
    m_cache.insert(WebPage, qnSkin->icon(kWebpageIcon));
    m_cache.insert(WebPageProxied, qnSkin->icon(kWebpageProxiedIcon));

    // Analytics.
    m_cache.insert(AnalyticsEngine, qnSkin->icon(kServerIcon));
    m_cache.insert(AnalyticsEngines, qnSkin->icon(kServersIcon));
    m_cache.insert(AnalyticsEngine | Offline, qnSkin->icon(kServerOfflineIcon));

    // Client.
    m_cache.insert(Client, qnSkin->icon(kClientIcon));

    // Cloud system.
    m_cache.insert(CloudSystem, qnSkin->icon(kSystemCloudIcon));
    m_cache.insert(CloudSystem | Offline, qnSkin->icon(kSystemCloudOfflineIcon));
    m_cache.insert(CloudSystem | Locked, qnSkin->icon(kSystemCloudWarningIcon));
    m_cache.insert(CloudSystem | Incompatible, qnSkin->icon(kSystemCloudUnauthIcon));
}

QnResourceIconCache::~QnResourceIconCache()
{
}

QnResourceIconCache* QnResourceIconCache::instance()
{
    return qn_resourceIconCache();
}

QIcon QnResourceIconCache::icon(Key key)
{
    /* This function will be called from GUI thread only,
     * so no synchronization is needed. */

    if ((key & TypeMask) == Unknown)
        key = Unknown;

    if (m_cache.contains(key))
        return m_cache.value(key);

    QIcon result;

    if (key & AlwaysSelected)
    {
        QIcon source = icon(key & ~AlwaysSelected);
        for (const QSize& size : source.availableSizes(QIcon::Selected))
        {
            QPixmap selectedPixmap = source.pixmap(size, QIcon::Selected);
            result.addPixmap(selectedPixmap, QIcon::Normal);
            result.addPixmap(selectedPixmap, QIcon::Selected);
        }
    }
    else
    {
        const auto base = key & TypeMask;
        const auto status = key & StatusMask;
        if (status != QnResourceIconCache::Online)
        {
            NX_ASSERT(false, "All icons should be pre-generated.");
        }

        result = m_cache.value(base);
    }

    m_cache.insert(key, result);
    return result;
}

QIcon QnResourceIconCache::icon(const QnResourcePtr& resource)
{
    if (!resource)
        return QIcon();
    return icon(key(resource));
}
QnResourceIconCache::Key QnResourceIconCache::userKey(const QnUserResourcePtr& user)
{
    if (!NX_ASSERT(user))
        return User;

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

QnResourceIconCache::Key QnResourceIconCache::key(const QnResourcePtr& resource)
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
    else if (flags.testFlag(Qn::server_archive))
    {
        NX_ASSERT(false, "What's that actually?");
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

        if (!ini().webPagesAndIntegrations)
            options.setFlag(QnWebPageResource::Integration, false); //< Use regular Web Page icon.

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

    if (auto crossSystemCamera = resource.dynamicCast<CrossSystemCameraResource>();
        crossSystemCamera && flags.testFlag(Qn::fake))
    {
        const auto systemId = crossSystemCamera->systemId();
        const auto context = appContext()->cloudCrossSystemManager()->systemContext(systemId);
        if (NX_ASSERT(context))
        {
            const auto systemStatus = context->status();
            if (systemStatus == CloudCrossSystemContext::Status::connecting)
            {
                key = CrossSystemStatus;
                status = QnResourceIconCache::Control;
            }
            else if (systemStatus == CloudCrossSystemContext::Status::connectionFailure
                && context->systemDescription()->isOnline())
            {
                status = QnResourceIconCache::Unauthorized;
            }
        }
    }
    else if (const auto layout = resource.dynamicCast<LayoutResource>())
    {
        const bool isVideoWallReviewLayout = layout->isVideoWallReviewLayout();

        if (isVideoWallReviewLayout)
            key = VideoWall;
        else if (layout->layoutType() == LayoutResource::LayoutType::intercom)
            key = IntercomLayout;
        else if (layout->isCrossSystem())
            key = CloudLayout;
        else if (layout->isShared())
            key = SharedLayout;
        else if (layout::isEncrypted(layout))
            key = ExportedEncryptedLayout;
        else if (layout->isFile())
            key = ExportedLayout;
        else
            key = Layout;

        status = Unknown;
    }
    else if (resource->hasFlags(Qn::virtual_camera))
    {
        status = Online;
    }
    else if (const auto camera = resource.dynamicCast<QnSecurityCamResource>())
    {
        const auto isBuggy = getResourceExtraStatus(camera).testFlag(ResourceExtraStatusFlag::buggy);
        if (status == Online && (camera->needsToChangeDefaultPassword() || isBuggy))
            status = Incompatible;
    }

    if (flags.testFlag(Qn::read_only))
        status |= ReadOnly;

    return Key(key | status);
}

QIcon QnResourceIconCache::cameraRecordingStatusIcon(ResourceExtraStatus status)
{
    if (status.testFlag(ResourceExtraStatusFlag::recording))
        return qnSkin->icon(kRecordOnIcon);

    if (status.testFlag(ResourceExtraStatusFlag::scheduled))
        return qnSkin->icon(kRecordPartIcon);

    if (status.testFlag(ResourceExtraStatusFlag::hasArchive))
        return qnSkin->icon(kHasArchiveIcon);

    return QIcon();
}
