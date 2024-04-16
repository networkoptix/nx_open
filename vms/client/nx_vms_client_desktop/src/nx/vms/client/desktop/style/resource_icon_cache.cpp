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
    // Systems.
    m_paths.insert(CurrentSystem, "20x20/Solid/system_local.svg");
    // TODO: pprivalov ask dbatov how it should looks actually
    m_paths.insert(OtherSystem, "20x20/Solid/system_local.svg");
    m_paths.insert(OtherSystems, "20x20/Solid/other_systems.svg");

    // Servers.
    m_paths.insert(Servers, "20x20/Solid/servers.svg");
    m_paths.insert(Server, "20x20/Solid/server.svg");
    m_paths.insert(Server | Offline, "20x20/Solid/server_offline.svg");
    m_paths.insert(Server | Incompatible, "20x20/Solid/server_incompatible.svg");
    m_paths.insert(Server | Control, "20x20/Solid/server_current.svg");
    m_paths.insert(Server | Unauthorized, "20x20/Solid/server_unauthorized.svg");
    // TODO: separate icon for Server with mismatched certificate.
    // TODO: pprivalov check that it works correctly with svg and if previous todo is still valid
    m_paths.insert(Server | MismatchedCertificate, "20x20/Solid/server_incompatible.svg");
    // Read-only server that is auto-discovered.
    // TODO: pprivalov check that correct collor is used
    m_paths.insert(Server | Incompatible | ReadOnly,
        "20x20/Solid/server_incompatible.svg");
    // Read-only server we are connected to.
    m_paths.insert(Server | Control | ReadOnly, "20x20/Solid/server_incompatible.svg");
    m_paths.insert(HealthMonitor, "20x20/Solid/health_monitor.svg");
    m_paths.insert(HealthMonitor | Offline, "20x20/Solid/health_monitor_offline.svg");

    // Layouts.
    m_paths.insert(Layouts, "20x20/Solid/layouts.svg");
    m_paths.insert(Layout, "20x20/Solid/layout.svg");
    m_paths.insert(ExportedLayout, "20x20/Solid/layout.svg");
    m_paths.insert(ExportedEncryptedLayout, "20x20/Solid/layout_exported_encrypted.svg");
    m_paths.insert(IntercomLayout, "20x20/Solid/layouts_intercom.svg");
    m_paths.insert(SharedLayout, "20x20/Solid/layout_shared.svg");
    m_paths.insert(CloudLayout, "20x20/Solid/layout_cloud.svg");
    m_paths.insert(SharedLayouts, "20x20/Solid/layouts_shared.svg");
    m_paths.insert(Showreel, "20x20/Solid/layout_tour.svg");
    m_paths.insert(Showreels, "20x20/Solid/layout_tours.svg");

    // Cameras.
    m_paths.insert(Cameras, "20x20/Solid/cameras.svg");
    m_paths.insert(Camera, "20x20/Solid/camera.svg");
    m_paths.insert(Camera | Offline, "20x20/Solid/camera_offline.svg");
    m_paths.insert(Camera | Unauthorized, "20x20/Solid/camera_unauthorized.svg");
    m_paths.insert(Camera | Incompatible, "20x20/Solid/camera_warning.svg");
    m_paths.insert(VirtualCamera, "20x20/Solid/virtual_camera.svg");
    m_paths.insert(CrossSystemStatus | Unauthorized, "events/alert_yellow.png");
    m_paths.insert(CrossSystemStatus | Control, "legacy/loading.gif"); //< The Control uses to describe loading state.
    m_paths.insert(CrossSystemStatus | Offline, "cloud/cloud_20_disabled.png");
    m_paths.insert(IOModule, "20x20/Solid/io.svg");
    m_paths.insert(IOModule | Offline, "20x20/Solid/io_offline.svg");
    m_paths.insert(IOModule | Unauthorized, "20x20/Solid/io_unauthorized.svg");
    m_paths.insert(IOModule | Incompatible, "20x20/Solid/camera_warning.svg");
    m_paths.insert(Recorder, "20x20/Solid/encoder.svg");
    m_paths.insert(MultisensorCamera, "20x20/Solid/multisensor.svg");

    // Local files.
    m_paths.insert(LocalResources, "20x20/Solid/ws_folder_open.svg");
    m_paths.insert(Image, "20x20/Solid/snapshot.svg");
    m_paths.insert(Image | Offline, "20x20/Solid/snapshot_offline.svg");
    m_paths.insert(Media, "20x20/Solid/media.svg");
    m_paths.insert(Media | Offline, "20x20/Solid/media_offline.svg");

    // Users.
    m_paths.insert(Users, "20x20/Solid/group.svg");
    m_paths.insert(User, "20x20/Solid/user.svg");
    m_paths.insert(CloudUser, "user_settings/user_cloud.svg");
    m_paths.insert(LdapUser, "user_settings/user_ldap.svg");
    m_paths.insert(LocalUser, "user_settings/user_local.svg");
    m_paths.insert(
        TemporaryUser, "user_settings/user_local_temp.svg");

    // Videowalls.
    m_paths.insert(VideoWall, "20x20/Solid/videowall.svg");
    m_paths.insert(VideoWallItem, "20x20/Solid/screen.svg");
    m_paths.insert(VideoWallItem | Locked, "20x20/Solid/screen_locked.svg");
    m_paths.insert(VideoWallItem | Control, "20x20/Solid/screen_controlled.svg");
    m_paths.insert(VideoWallItem | Offline, "20x20/Solid/screen_offline.svg");
    m_paths.insert(VideoWallMatrix, "20x20/Solid/matrix.svg");

    // Integrations.
    m_paths.insert(Integrations, "20x20/Solid/extensions.svg");
    m_paths.insert(Integration, "20x20/Solid/extension.svg");
    m_paths.insert(IntegrationProxied, "20x20/Solid/extension_proxied.svg");

    // Web Pages.
    m_paths.insert(WebPages, "20x20/Solid/webpages.svg");
    m_paths.insert(WebPage, "20x20/Solid/webpage.svg");
    m_paths.insert(WebPageProxied, "20x20/Solid/webpage_proxied.svg");

    // Analytics.
    m_paths.insert(AnalyticsEngine, "20x20/Solid/server.svg");
    m_paths.insert(AnalyticsEngines, "20x20/Solid/servers.svg");
    m_paths.insert(AnalyticsEngine | Offline, "20x20/Solid/server_offline.svg");

    // Client.
    m_paths.insert(Client, "20x20/Solid/client.svg");

    // Cloud system.
    m_paths.insert(CloudSystem, "tree/cloud_system.svg");
    m_paths.insert(CloudSystem | Offline, "tree/cloud_system_offline.svg");
    m_paths.insert(CloudSystem | Locked, "tree/cloud_system_warning.svg");
    m_paths.insert(CloudSystem | Incompatible, "tree/cloud_system_incompatible.svg");

    m_cache.insert(Unknown, QIcon());

    for (const auto& [key, path]: std::as_const(m_paths).asKeyValueRange())
        m_cache.insert(key, loadIcon(path, kTreeThemeSubstitutions));
}

QnResourceIconCache::~QnResourceIconCache()
{
}

QnResourceIconCache* QnResourceIconCache::instance()
{
    return qn_resourceIconCache();
}

QString QnResourceIconCache::iconPath(Key key) const
{
    auto it = m_paths.find(key);
    if (it != m_paths.cend())
        return *it;

    it = m_paths.find(key & (TypeMask | StatusMask));
    if (it != m_paths.cend())
        return *it;

    it = m_paths.find(key & TypeMask);
    return NX_ASSERT(it != m_paths.cend()) ? (*it) : QString{};
}

QString QnResourceIconCache::iconPath(const QnResourcePtr& resource) const
{
    return iconPath(key(resource));
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
        return qnSkin->icon("20x20/Solid/record_on.svg");

    if (status.testFlag(ResourceExtraStatusFlag::scheduled))
        return qnSkin->icon("20x20/Solid/record_part.svg");

    if (status.testFlag(ResourceExtraStatusFlag::hasArchive))
        return qnSkin->icon("20x20/Solid/archive.svg");

    return QIcon();
}
