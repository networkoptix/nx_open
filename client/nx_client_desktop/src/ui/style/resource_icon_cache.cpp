#include "resource_icon_cache.h"

#include <QtGui/QPixmap>
#include <QtGui/QPainter>

#include <client/client_globals.h>

#include <common/common_module.h>

#include <core/resource_management/resource_runtime_data.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>

#include <ui/style/skin.h>
#include <api/global_settings.h>
#include <network/system_helpers.h>

#include <nx/fusion/model_functions.h>

Q_GLOBAL_STATIC(QnResourceIconCache, qn_resourceIconCache);

namespace {

const QString customKeyProperty = lit("customIconCacheKey");

QIcon loadIcon(const QString& name)
{
    static const QnIcon::SuffixesList kResourceIconSuffixes({
        {QnIcon::Active,   lit("accented")},
        {QnIcon::Disabled, lit("disabled")},
        {QnIcon::Selected, lit("selected")},
    });

    return qnSkin->icon(name, QString(), &kResourceIconSuffixes);
}

#define QN_STRINGIFY(VALUE) case QnResourceIconCache::VALUE: return QLatin1String(BOOST_PP_STRINGIZE(VALUE))

QString baseToString(QnResourceIconCache::Key base)
{
    switch (base)
    {
        QN_STRINGIFY(Unknown);
        QN_STRINGIFY(LocalResources);
        QN_STRINGIFY(CurrentSystem);

        QN_STRINGIFY(Server);
        QN_STRINGIFY(Servers);
        QN_STRINGIFY(HealthMonitor);

        QN_STRINGIFY(Layout);
        QN_STRINGIFY(SharedLayout);
        QN_STRINGIFY(Layouts);
        QN_STRINGIFY(SharedLayouts);

        QN_STRINGIFY(Camera);
        QN_STRINGIFY(Cameras);

        QN_STRINGIFY(Recorder);
        QN_STRINGIFY(Image);
        QN_STRINGIFY(Media);
        QN_STRINGIFY(User);
        QN_STRINGIFY(Users);
        QN_STRINGIFY(VideoWall);
        QN_STRINGIFY(VideoWallItem);
        QN_STRINGIFY(VideoWallMatrix);

        QN_STRINGIFY(OtherSystem);
        QN_STRINGIFY(OtherSystems);

        QN_STRINGIFY(IOModule);
        QN_STRINGIFY(WebPage);
        QN_STRINGIFY(WebPages);
        default:
            return QString::number(base);
    };
}

QString statusToString(QnResourceIconCache::Key status)
{
    switch (status)
    {
        QN_STRINGIFY(Offline);
        QN_STRINGIFY(Unauthorized);
        QN_STRINGIFY(Online);
        QN_STRINGIFY(Locked);
        QN_STRINGIFY(Incompatible);
        QN_STRINGIFY(Control);
        default:
            return QString::number(status >> 8);
    };
}

QString keyToString(QnResourceIconCache::Key key)
{
    QString basePart = baseToString(key & QnResourceIconCache::TypeMask);
    QString statusPart = statusToString(key & QnResourceIconCache::StatusMask);
    return basePart + L':' + statusPart;
}

#undef QN_STRINGIFY

} //namespace

QnResourceIconCache::QnResourceIconCache(QObject* parent): QObject(parent)
{
    m_cache.insert(Unknown,                 QIcon());
    m_cache.insert(LocalResources,          loadIcon(lit("tree/local.png")));
    m_cache.insert(CurrentSystem,           loadIcon(lit("tree/system.png")));
    m_cache.insert(Server,                  loadIcon(lit("tree/server.png")));
    m_cache.insert(Servers,                 loadIcon(lit("tree/servers.png")));
    m_cache.insert(HealthMonitor,           loadIcon(lit("tree/health_monitor.png")));
    m_cache.insert(Layout,                  loadIcon(lit("tree/layout.png")));
    m_cache.insert(SharedLayout,            loadIcon(lit("tree/layout_shared.png")));
    m_cache.insert(Layouts,                 loadIcon(lit("tree/layouts.png")));
    m_cache.insert(SharedLayouts,           loadIcon(lit("tree/layouts_shared.png")));
    m_cache.insert(LayoutTour,              loadIcon(lit("tree/layout_tour.png")));
    m_cache.insert(LayoutTours,             loadIcon(lit("tree/layout_tours.png")));
    m_cache.insert(Camera,                  loadIcon(lit("tree/camera.png")));
    m_cache.insert(Cameras,                 loadIcon(lit("tree/cameras.png")));
    m_cache.insert(IOModule,                loadIcon(lit("tree/io.png")));
    m_cache.insert(Recorder,                loadIcon(lit("tree/encoder.png")));
    m_cache.insert(Image,                   loadIcon(lit("tree/snapshot.png")));
    m_cache.insert(Media,                   loadIcon(lit("tree/media.png")));
    m_cache.insert(User,                    loadIcon(lit("tree/user.png")));
    m_cache.insert(Users,                   loadIcon(lit("tree/users.png")));
    m_cache.insert(VideoWall,               loadIcon(lit("tree/videowall.png")));
    m_cache.insert(VideoWallItem,           loadIcon(lit("tree/screen.png")));
    m_cache.insert(VideoWallMatrix,         loadIcon(lit("tree/matrix.png")));
    m_cache.insert(OtherSystem,             loadIcon(lit("tree/other_systems.png")));
    m_cache.insert(OtherSystems,            loadIcon(lit("tree/other_systems.png")));
    m_cache.insert(WebPage,                 loadIcon(lit("tree/webpage.png")));
    m_cache.insert(WebPages,                loadIcon(lit("tree/webpages.png")));

    m_cache.insert(Media | Offline,         loadIcon(lit("tree/media_offline.png")));
    m_cache.insert(Image | Offline,         loadIcon(lit("tree/snapshot_offline.png")));
    m_cache.insert(Server | Offline,        loadIcon(lit("tree/server_offline.png")));
    m_cache.insert(Server | Incompatible,   loadIcon(lit("tree/server_incompatible.png")));
    m_cache.insert(Server | Control,        loadIcon(lit("tree/server_current.png")));
    m_cache.insert(Server | Unauthorized,   loadIcon(lit("tree/server_unauthorized.png")));
    m_cache.insert(HealthMonitor| Offline,  loadIcon(lit("tree/health_monitor_offline.png")));
    m_cache.insert(Camera | Offline,        loadIcon(lit("tree/camera_offline.png")));
    m_cache.insert(Camera | Unauthorized,   loadIcon(lit("tree/camera_unauthorized.png")));
    m_cache.insert(Camera | Incompatible,   loadIcon(lit("tree/camera_alert.png")));
    m_cache.insert(IOModule | Incompatible, loadIcon(lit("tree/camera_alert.png")));
    m_cache.insert(Layout | Locked,         loadIcon(lit("tree/layout_locked.png")));
    m_cache.insert(SharedLayout | Locked,   loadIcon(lit("tree/layout_shared_locked.png")));
    m_cache.insert(VideoWallItem | Locked,  loadIcon(lit("tree/screen_locked.png")));
    m_cache.insert(VideoWallItem | Control, loadIcon(lit("tree/screen_controlled.png")));
    m_cache.insert(VideoWallItem | Offline, loadIcon(lit("tree/screen_offline.png")));
    m_cache.insert(IOModule | Offline,      loadIcon(lit("tree/io_offline.png")));
    m_cache.insert(IOModule | Unauthorized, loadIcon(lit("tree/io_unauthorized.png")));
    m_cache.insert(WebPage | Offline,       loadIcon(lit("tree/webpage_offline.png")));

    /* Read-only server that is auto-discovered. */
    m_cache.insert(Server | Incompatible | ReadOnly,    loadIcon(lit("tree/server_incompatible_disabled.png")));
    /* Read-only server we are connected to. */
    m_cache.insert(Server | Control | ReadOnly,         loadIcon(lit("tree/server_incompatible.png")));
}

QnResourceIconCache::~QnResourceIconCache()
{
}

QnResourceIconCache* QnResourceIconCache::instance()
{
    return qn_resourceIconCache();
}

QIcon QnResourceIconCache::icon(Key key, bool unchecked)
{
    /* This function will be called from GUI thread only,
     * so no synchronization is needed. */

    if ((key & TypeMask) == Unknown && !unchecked)
        key = Unknown;

    if (m_cache.contains(key))
        return m_cache.value(key);

    const auto base = key & TypeMask;
    const auto status = key & StatusMask;
    if (status != QnResourceIconCache::Online)
    {
        qDebug() << "Error: unknown icon" << keyToString(key);
        NX_ASSERT(false, Q_FUNC_INFO, "All icons should be pre-generated.");
    }

    QIcon icon = m_cache.value(base);

    m_cache.insert(key, icon);
    return icon;
}

QIcon QnResourceIconCache::icon(const QnResourcePtr& resource)
{
    if (!resource)
        return QIcon();
    return icon(key(resource));
}

void QnResourceIconCache::setKey(const QnResourcePtr& resource, Key key)
{
    qnResourceRuntimeDataManager->setResourceData(resource, Qn::ResourceIconKeyRole, (int)key);
}

void QnResourceIconCache::clearKey(const QnResourcePtr& resource)
{
    qnResourceRuntimeDataManager->cleanupResourceData(resource, Qn::ResourceIconKeyRole);
}

QnResourceIconCache::Key QnResourceIconCache::key(const QnResourcePtr& resource)
{
    Key key = Unknown;

    auto customIconKey = qnResourceRuntimeDataManager->resourceData(resource, Qn::ResourceIconKeyRole);
    if (customIconKey.isValid())
    {
        bool ok = true;
        key = static_cast<Key>(customIconKey.toInt());
        if (ok)
            return key;
    }

    Qn::ResourceFlags flags = resource->flags();
    if (flags.testFlag(Qn::local_server))
        key = LocalResources;
    else if (flags.testFlag(Qn::server))
        key = Server;
    else if (flags.testFlag(Qn::layout))
        key = Layout;
    else if (flags.testFlag(Qn::io_module))
        key = IOModule;
    else if (flags.testFlag(Qn::live_cam))
        key = Camera;
    else if (flags.testFlag(Qn::local_image))
        key = Image;
    else if (flags.testFlag(Qn::local_video))
        key = Media;
    else if (flags.testFlag(Qn::server_archive))
        key = Media;
    else if (flags.testFlag(Qn::user))
        key = User;
    else if (flags.testFlag(Qn::videowall))
        key = VideoWall;
    else if (flags.testFlag(Qn::web_page))
        key = WebPage;

    Key status = Unknown;

    const auto updateStatus =
        [&status, key, resource]()
        {
            switch (resource->getStatus())
            {
                case Qn::Online:
                    if (key == Server && resource->getId() == resource->commonModule()->remoteGUID())
                        status = Control;
                    else
                        status = Online;
                    break;

                case Qn::Offline:
                    status = Offline;
                    break;

                case Qn::Unauthorized:
                    status = Unauthorized;
                    break;

                case Qn::Incompatible:
                    status = Incompatible;
                    break;

                default:
                    break;
            };
        };

    // Fake servers
    if (flags.testFlag(Qn::fake))
    {
        auto server = resource.dynamicCast<QnMediaServerResource>();
        NX_ASSERT(server);
        status = helpers::serverBelongsToCurrentSystem(server)
            ? Incompatible
            : Online;
    }
    else if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        const bool videowall = !layout->data().value(Qn::VideoWallResourceRole)
            .value<QnVideoWallResourcePtr>().isNull();

        if (videowall)
            key = VideoWall;
        else if (layout->isShared())
            key = SharedLayout;

        status = (layout->locked() && !videowall)
            ? Locked
            : Unknown;
    }
    else if (const auto camera = resource.dynamicCast<QnSecurityCamResource>())
    {
        updateStatus();
        if (status == Online && camera->needsToChangeDefaultPassword())
            status = Incompatible;
    }
    else
    {
        updateStatus();
    }

    if (flags.testFlag(Qn::read_only))
        status |= ReadOnly;

    return Key(key | status);
}
