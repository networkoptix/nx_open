// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_icon_cache.h"

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/fake_media_server.h>
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
    m_cache.insert(CurrentSystem, loadIcon("tree/system_local.svg"));
    // TODO: pprivalov ask dbatov how it should looks actually
    m_cache.insert(OtherSystem, loadIcon("tree/system_local.svg"));
    m_cache.insert(OtherSystems, loadIcon("tree/other_systems.svg"));

    // Servers.
    m_cache.insert(Servers, loadIcon("tree/servers.svg"));
    m_cache.insert(Server, loadIcon("tree/server.svg"));
    m_cache.insert(Server | Offline, loadIcon("tree/server_offline.svg"));
    m_cache.insert(Server | Incompatible, loadIcon("tree/server_incompatible.svg"));
    m_cache.insert(Server | Control, loadIcon("tree/server_current.svg"));
    m_cache.insert(Server | Unauthorized, loadIcon("tree/server_unauthorized.svg"));
    // TODO: separate icon for Server with mismatched certificate.
    // TODO: pprivalov check that it works correctly with svg and if previous todo is still valid
    m_cache.insert(Server | MismatchedCertificate, loadIcon("tree/server_incompatible.svg"));
    // Read-only server that is auto-discovered.
    // TODO: pprivalov check that correct collor is used
    m_cache.insert(Server | Incompatible | ReadOnly,
        loadIcon("tree/server_incompatible.svg"));
    // Read-only server we are connected to.
    m_cache.insert(Server | Control | ReadOnly, loadIcon("tree/server_incompatible.svg"));
    m_cache.insert(HealthMonitor, loadIcon("tree/health_monitor.svg"));
    m_cache.insert(HealthMonitor | Offline, loadIcon("tree/health_monitor_offline.svg"));

    // Layouts.
    m_cache.insert(Layouts, loadIcon("tree/layouts.svg"));
    m_cache.insert(Layout, loadIcon("tree/layout.svg"));
    m_cache.insert(ExportedLayout, loadIcon("tree/layout.svg"));
    m_cache.insert(ExportedEncryptedLayout, loadIcon("tree/layout_exported_encrypted.svg"));
    m_cache.insert(IntercomLayout, loadIcon("tree/layouts_intercom.svg"));
    m_cache.insert(SharedLayout, loadIcon("tree/layout_shared.svg"));
    m_cache.insert(CloudLayout, loadIcon("tree/layout_cloud.svg"));
    m_cache.insert(SharedLayouts, loadIcon("tree/layouts_shared.svg"));
    m_cache.insert(Showreel, loadIcon("tree/layout_tour_20.svg"));
    m_cache.insert(Showreels, loadIcon("tree/layout_tours_20.svg"));

    // Cameras.
    m_cache.insert(Cameras, loadIcon("tree/cameras.svg"));
    m_cache.insert(Camera, loadIcon("tree/camera.svg"));
    m_cache.insert(Camera | Offline, loadIcon("tree/camera_offline.svg"));
    m_cache.insert(Camera | Unauthorized, loadIcon("tree/camera_unauthorized.svg"));
    m_cache.insert(Camera | Incompatible, loadIcon("tree/camera_alert.svg"));
    m_cache.insert(VirtualCamera, loadIcon("tree/virtual_camera.svg", kTreeThemeSubstitutions));
    m_cache.insert(CrossSystemStatus | Unauthorized, loadIcon("events/alert_yellow.png"));
    m_cache.insert(CrossSystemStatus | Control, loadIcon("legacy/loading.gif")); //< The Control uses to describe loading state.
    m_cache.insert(CrossSystemStatus | Offline, loadIcon("cloud/cloud_20_disabled.png"));
    m_cache.insert(IOModule, loadIcon("tree/io.svg"));
    m_cache.insert(IOModule | Offline, loadIcon("tree/io_offline.svg"));
    m_cache.insert(IOModule | Unauthorized, loadIcon("tree/io_unauthorized.svg"));
    m_cache.insert(IOModule | Incompatible, loadIcon("tree/camera_alert.svg"));
    m_cache.insert(Recorder, loadIcon("tree/encoder.svg"));
    m_cache.insert(MultisensorCamera, loadIcon("tree/multisensor.svg"));

    // Local files.
    m_cache.insert(LocalResources, loadIcon("tree/local.svg"));
    m_cache.insert(Image, loadIcon("tree/snapshot.svg"));
    m_cache.insert(Image | Offline, loadIcon("tree/snapshot_offline.svg"));
    m_cache.insert(Media, loadIcon("tree/media.svg"));
    m_cache.insert(Media | Offline, loadIcon("tree/media_offline.svg"));

    // Users.
    m_cache.insert(Users, loadIcon("tree/users.svg", kTreeThemeSubstitutions));
    m_cache.insert(User, loadIcon("tree/user.svg", kTreeThemeSubstitutions));
    m_cache.insert(CloudUser, loadIcon("user_settings/user_cloud.svg", kTreeThemeSubstitutions));
    m_cache.insert(LdapUser, loadIcon("user_settings/user_ldap.svg", kTreeThemeSubstitutions));
    m_cache.insert(LocalUser, loadIcon("user_settings/user_local.svg", kTreeThemeSubstitutions));
    m_cache.insert(
        TemporaryUser, loadIcon("user_settings/user_local_temp.svg", kTreeThemeSubstitutions));

    // Videowalls.
    m_cache.insert(VideoWall, loadIcon("tree/videowall.svg"));
    m_cache.insert(VideoWallItem, loadIcon("tree/screen.svg"));
    m_cache.insert(VideoWallItem | Locked, loadIcon("tree/screen_locked.svg"));
    m_cache.insert(VideoWallItem | Control, loadIcon("tree/screen_controlled.svg"));
    m_cache.insert(VideoWallItem | Offline, loadIcon("tree/screen_offline.svg"));
    m_cache.insert(VideoWallMatrix, loadIcon("tree/matrix.svg"));

    // Integrations.
    m_cache.insert(Integrations, loadIcon("tree/integrations.svg"));
    m_cache.insert(Integration, loadIcon("tree/integration.svg"));
    m_cache.insert(IntegrationProxied, loadIcon("tree/integration_proxied.svg"));

    // Web Pages.
    m_cache.insert(WebPages, loadIcon("tree/webpages.svg"));
    m_cache.insert(WebPage, loadIcon("tree/webpage.svg"));
    m_cache.insert(WebPageProxied, loadIcon("tree/webpage_proxied.svg"));

    // Analytics.
    m_cache.insert(AnalyticsEngine, loadIcon("tree/server.svg"));
    m_cache.insert(AnalyticsEngines, loadIcon("tree/servers.svg"));
    m_cache.insert(AnalyticsEngine | Offline, loadIcon("tree/server_offline.svg"));

    // Client.
    m_cache.insert(Client, loadIcon("tree/client.svg"));

    // Cloud system.
    m_cache.insert(CloudSystem, loadIcon("tree/cloud_system.svg"));
    m_cache.insert(CloudSystem | Offline, loadIcon("tree/cloud_system_offline.svg"));
    m_cache.insert(CloudSystem | Locked, loadIcon("tree/cloud_system_warning.svg"));
    m_cache.insert(CloudSystem | Incompatible, loadIcon("tree/cloud_system_incompatible.svg"));
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
    // Fake servers
    else if (auto fakeServer = resource.dynamicCast<QnFakeMediaServerResource>())
    {
        status = helpers::serverBelongsToCurrentSystem(fakeServer)
            ? Incompatible
            : Online;
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
        return qnSkin->icon("tree/record_on.svg");

    if (status.testFlag(ResourceExtraStatusFlag::scheduled))
        return qnSkin->icon("tree/record_part.svg");

    if (status.testFlag(ResourceExtraStatusFlag::hasArchive))
        return qnSkin->icon("tree/archive.svg");

    return QIcon();
}
