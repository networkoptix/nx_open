// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_icon_cache.h"

#include <QtGui/QPainter>
#include <QtGui/QPixmap>

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_management/resource_runtime_data.h>
#include <network/system_helpers.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cross_system_camera_resource.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/system_settings.h>
#include <nx_ec/abstract_ec_connection.h>

using namespace nx::vms::client::desktop;

Q_GLOBAL_STATIC(QnResourceIconCache, qn_resourceIconCache);

namespace {

QIcon loadIcon(const QString& name)
{
    static const QnIcon::Suffixes kResourceIconSuffixes({
        {QnIcon::Active,   "accented"},
        {QnIcon::Disabled, "disabled"},
        {QnIcon::Selected, "selected"},
    });

    return qnSkin->icon(name, QString(), &kResourceIconSuffixes);
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
            auto connection = resource->systemContext()->ec2Connection();
            if (key == Key::Server
                && connection
                && resource->getId() == connection->moduleInformation().id)
            {
                return Status::Control;
            }

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

QnResourceIconCache::QnResourceIconCache(QObject* parent):
    QObject(parent)
{
    m_cache.insert(Unknown, QIcon());

    // Systems.
    m_cache.insert(CurrentSystem, loadIcon("tree/system.png"));
    m_cache.insert(OtherSystem, loadIcon("tree/other_systems.png"));
    m_cache.insert(OtherSystems, loadIcon("tree/other_systems.png"));

    // Servers.
    m_cache.insert(Servers, loadIcon("tree/servers.png"));
    m_cache.insert(Server, loadIcon("tree/server.png"));
    m_cache.insert(Server | Offline, loadIcon("tree/server_offline.png"));
    m_cache.insert(Server | Incompatible, loadIcon("tree/server_incompatible.png"));
    m_cache.insert(Server | Control, loadIcon("tree/server_current.png"));
    m_cache.insert(Server | Unauthorized, loadIcon("tree/server_unauthorized.png"));
    // TODO: separate icon for Server with mismatched certificate.
    m_cache.insert(Server | MismatchedCertificate, loadIcon("tree/server_incompatible_disabled.png"));
    // Read-only server that is auto-discovered.
    m_cache.insert(Server | Incompatible | ReadOnly,
        loadIcon("tree/server_incompatible_disabled.png"));
    // Read-only server we are connected to.
    m_cache.insert(Server | Control | ReadOnly, loadIcon("tree/server_incompatible.png"));
    m_cache.insert(HealthMonitor, loadIcon("tree/health_monitor.png"));
    m_cache.insert(HealthMonitor | Offline, loadIcon("tree/health_monitor_offline.png"));

    // Layouts.
    m_cache.insert(Layouts, loadIcon("tree/layouts.svg"));
    m_cache.insert(Layout, loadIcon("tree/layout.svg"));
    m_cache.insert(Layout | Locked, loadIcon("tree/layout_locked.svg"));
    m_cache.insert(ExportedLayout, loadIcon("tree/layout_exported.svg"));
    m_cache.insert(ExportedLayout | Locked, loadIcon("tree/layout_exported_locked.svg"));
    m_cache.insert(ExportedEncryptedLayout, loadIcon("tree/layout_exported_encrypted.svg"));
    m_cache.insert(IntercomLayout, loadIcon("tree/layouts_intercom.svg"));
    m_cache.insert(SharedLayout, loadIcon("tree/layout_shared.svg"));
    m_cache.insert(SharedLayout | Locked, loadIcon("tree/layout_shared_locked.svg"));
    m_cache.insert(CloudLayout, loadIcon("tree/layout_cloud.svg"));
    m_cache.insert(CloudLayout | Locked, loadIcon("tree/layout_cloud_locked.svg"));
    m_cache.insert(SharedLayouts, loadIcon("tree/layouts_shared.svg"));
    m_cache.insert(LayoutTour, loadIcon("tree/layout_tour.png"));
    m_cache.insert(LayoutTours, loadIcon("tree/layout_tours.png"));

    // Cameras.
    m_cache.insert(Cameras, loadIcon("tree/cameras.png"));
    m_cache.insert(Camera, loadIcon("tree/camera.svg"));
    m_cache.insert(Camera | Offline, loadIcon("tree/camera_offline.svg"));
    m_cache.insert(Camera | Unauthorized, loadIcon("tree/camera_unauthorized.svg"));
    m_cache.insert(Camera | Incompatible, loadIcon("tree/camera_alert.svg"));
    m_cache.insert(VirtualCamera, loadIcon("tree/virtual_camera.png"));
    m_cache.insert(CrossSystemStatus | Unauthorized, loadIcon("events/alert_yellow.png"));
    m_cache.insert(CrossSystemStatus | Control, loadIcon("legacy/loading.gif")); //< The Control uses to describe loading state.
    m_cache.insert(CrossSystemStatus | Offline, loadIcon("cloud/cloud_20_disabled.png"));
    m_cache.insert(IOModule, loadIcon("tree/io.png"));
    m_cache.insert(IOModule | Offline, loadIcon("tree/io_offline.png"));
    m_cache.insert(IOModule | Unauthorized, loadIcon("tree/io_unauthorized.png"));
    m_cache.insert(IOModule | Incompatible, loadIcon("tree/camera_alert.svg"));
    m_cache.insert(Recorder, loadIcon("tree/encoder.png"));
    m_cache.insert(MultisensorCamera, loadIcon("tree/multisensor.svg"));

    // Local files.
    m_cache.insert(LocalResources, loadIcon("tree/local.png"));
    m_cache.insert(Image, loadIcon("tree/snapshot.png"));
    m_cache.insert(Image | Offline, loadIcon("tree/snapshot_offline.png"));
    m_cache.insert(Media, loadIcon("tree/media.png"));
    m_cache.insert(Media | Offline, loadIcon("tree/media_offline.png"));

    // Users.
    m_cache.insert(Users, loadIcon("tree/users.png"));
    m_cache.insert(User, loadIcon("tree/user.png"));

    // Videowalls.
    m_cache.insert(VideoWall, loadIcon("tree/videowall.png"));
    m_cache.insert(VideoWallItem, loadIcon("tree/screen.png"));
    m_cache.insert(VideoWallItem | Locked, loadIcon("tree/screen_locked.png"));
    m_cache.insert(VideoWallItem | Control, loadIcon("tree/screen_controlled.png"));
    m_cache.insert(VideoWallItem | Offline, loadIcon("tree/screen_offline.png"));
    m_cache.insert(VideoWallMatrix, loadIcon("tree/matrix.png"));

    // Web Pages.
    m_cache.insert(WebPages, loadIcon("tree/webpages.png"));
    m_cache.insert(WebPage, loadIcon("tree/webpage.png"));
    m_cache.insert(WebPageProxied, loadIcon("tree/webpage_proxied.svg"));

    // Analytics.
    m_cache.insert(AnalyticsEngine, loadIcon("tree/server.png"));
    m_cache.insert(AnalyticsEngines, loadIcon("tree/servers.png"));
    m_cache.insert(AnalyticsEngine | Offline, loadIcon("tree/server_offline.png"));

    // Client.
    m_cache.insert(Client, loadIcon("tree/client.svg"));

    // Cloud system.
    m_cache.insert(CloudSystem, loadIcon("cloud/cloud_20.png"));
    m_cache.insert(CloudSystem | ReadOnly, loadIcon("cloud/cloud_20_disabled.png"));
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

void QnResourceIconCache::setKey(const QnResourcePtr& resource, Key key)
{
    SystemContext::fromResource(resource)->resourceRuntimeDataManager()
        ->setResourceData(resource, Qn::ResourceIconKeyRole, (int)key);
}

void QnResourceIconCache::clearKey(const QnResourcePtr& resource)
{
    SystemContext::fromResource(resource)->resourceRuntimeDataManager()
        ->cleanupResourceData(resource, Qn::ResourceIconKeyRole);
}

QnResourceIconCache::Key QnResourceIconCache::key(const QnResourcePtr& resource)
{
    Key key = Unknown;

    // #vbreus There is no client module in the testing environment.
    // But it seems that there nothing interesting regarding Resource Tree model.
    if (auto context = SystemContext::fromResource(resource))
    {
        auto customIconKey = context->resourceRuntimeDataManager()
            ->resourceData(resource, Qn::ResourceIconKeyRole);
        if (customIconKey.isValid())
            return static_cast<Key>(customIconKey.toInt());
    }

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
        key = User;
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
        NX_ASSERT(webPage);
        return webPage && !webPage->getProxyId().isNull() ? Key(WebPageProxied) : Key(WebPage);
    }

    Key status = calculateStatus(key, resource);

    if (flags.testFlag(Qn::cross_system) && flags.testFlag(Qn::fake))
    {
        key = CrossSystemStatus;
        const auto crossSystemCamera = resource.dynamicCast<nx::vms::client::desktop::CrossSystemCameraResource>();
        const auto systemStatus = NX_ASSERT(crossSystemCamera)
            ? crossSystemCamera->crossSystemContext()->status()
            : CloudCrossSystemContext::Status::uninitialized;

        if (systemStatus == CloudCrossSystemContext::Status::uninitialized)
            status = QnResourceIconCache::Offline;
        else if (systemStatus == CloudCrossSystemContext::Status::connecting)
            status = QnResourceIconCache::Control;
        else if (systemStatus == CloudCrossSystemContext::Status::connectionFailure)
            status = QnResourceIconCache::Unauthorized;
    }
    // Fake servers
    else if (flags.testFlag(Qn::fake_server))
    {
        auto server = resource.dynamicCast<QnMediaServerResource>();
        NX_ASSERT(server);
        status = helpers::serverBelongsToCurrentSystem(server)
            ? Incompatible
            : Online;
    }
    else if (const auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        const bool videowall = !layout->data().value(Qn::VideoWallResourceRole)
            .value<QnVideoWallResourcePtr>().isNull();

        if (videowall)
            key = VideoWall;
        else if (nx::vms::common::isIntercomLayout(layout))
            key = IntercomLayout;
        else if (layout->isShared())
            key = SharedLayout;
        else if (layout::isEncrypted(layout))
            key = ExportedEncryptedLayout;
        else if (layout->isFile())
            key = ExportedLayout;
        else
            key = Layout;

        status = (layout->locked() && !videowall)
            ? Locked
            : Unknown;
    }
    else if (resource->hasFlags(Qn::virtual_camera))
    {
        status = Online;
    }
    else if (const auto camera = resource.dynamicCast<QnSecurityCamResource>())
    {
        if (status == Online && camera->needsToChangeDefaultPassword())
            status = Incompatible;
    }

    if (flags.testFlag(Qn::read_only))
        status |= ReadOnly;

    return Key(key | status);
}

QIcon QnResourceIconCache::cameraRecordingStatusIcon(CameraExtraStatus status)
{
    if (status.testFlag(CameraExtraStatusFlag::recording))
        return qnSkin->icon("tree/recording.png");

    if (status.testFlag(CameraExtraStatusFlag::scheduled))
        return qnSkin->icon("tree/scheduled.png");

    if (status.testFlag(CameraExtraStatusFlag::hasArchive))
        return qnSkin->icon("tree/has_archive.png");

    return QIcon();
}
