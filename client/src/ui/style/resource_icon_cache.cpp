#include "resource_icon_cache.h"

#include <QtGui/QPixmap>
#include <QtGui/QPainter>

#include <common/common_module.h>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>

#include <ui/style/skin.h>
#include "core/resource/camera_resource.h"

Q_GLOBAL_STATIC(QnResourceIconCache, qn_resourceIconCache);

namespace
{
    const QString customKeyProperty = lit("customIconCacheKey");

    QIcon loadIcon(const QString& name)
    {
        static const QVector<QPair<QIcon::Mode, QString> > kResourceIconModes({
            qMakePair(QnIcon::Active,   lit("accented")),
            qMakePair(QnIcon::Disabled, lit("disabled")),
            qMakePair(QnIcon::Selected, lit("selected")),
        });

        return qnSkin->icon(name, QString(), kResourceIconModes.size(), &kResourceIconModes.front());
    }
}

QnResourceIconCache::QnResourceIconCache(QObject* parent): QObject(parent)
{
    m_cache.insert(Unknown,                 QIcon());
    m_cache.insert(LocalResources,          loadIcon(lit("tree/local.png")));
    m_cache.insert(CurrentSystem,           loadIcon(lit("tree/system.png")));
    m_cache.insert(Server,                  loadIcon(lit("tree/server.png")));
    m_cache.insert(Servers,                 loadIcon(lit("tree/servers.png")));
    m_cache.insert(Layout,                  loadIcon(lit("tree/layout.png")));
    m_cache.insert(SharedLayout,            loadIcon(lit("tree/layout_shared.png")));
    m_cache.insert(Layouts,                 loadIcon(lit("tree/layouts.png")));
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
    m_cache.insert(OtherSystem,             loadIcon(lit("tree/system.png")));
    m_cache.insert(OtherSystems,            loadIcon(lit("tree/other_systems.png")));
    m_cache.insert(WebPage,                 loadIcon(lit("tree/webpage.png")));
    m_cache.insert(WebPages,                loadIcon(lit("tree/webpages.png")));

    m_cache.insert(Media | Offline,         loadIcon(lit("tree/media_offline.png")));
    m_cache.insert(Image | Offline,         loadIcon(lit("tree/snapshot_offline.png")));
    m_cache.insert(Server | Offline,        loadIcon(lit("tree/server_offline.png")));
    m_cache.insert(Server | Incompatible,   loadIcon(lit("tree/server_incompatible.png")));
    m_cache.insert(Server | Control,        loadIcon(lit("tree/server_current.png")));
    m_cache.insert(Server | Unauthorized,   loadIcon(lit("tree/server_unauthorized.png")));
    m_cache.insert(Camera | Offline,        loadIcon(lit("tree/camera_offline.png")));
    m_cache.insert(Camera | Unauthorized,   loadIcon(lit("tree/camera_unauthorized.png")));
    m_cache.insert(Layout | Locked,         loadIcon(lit("tree/layout_locked.png")));
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

    /* Overlays. Should not be used. */
    m_cache.insert(Offline,                 loadIcon(lit("tree/offline.png")));
    m_cache.insert(Unauthorized,            loadIcon(lit("tree/unauthorized.png")));
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

    QIcon icon = m_cache.value(key & TypeMask);
    QIcon overlay = m_cache.value(key & StatusMask);
    if (!icon.isNull() && !overlay.isNull())
    {
        NX_ASSERT(false, Q_FUNC_INFO, "All icons should be pre-generated.");

        QPixmap pixmap = icon.pixmap(icon.actualSize(QSize(1024, 1024)));
        {
            QPainter painter(&pixmap);
            QRect r = pixmap.rect();
            r.setTopRight(r.center());
            overlay.paint(&painter, r, Qt::AlignLeft | Qt::AlignBottom);
        }
        icon = QIcon(pixmap);
    }

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
    resource->setProperty(customKeyProperty, QString::number(static_cast<int>(key), 16));
}

QnResourceIconCache::Key QnResourceIconCache::key(const QnResourcePtr& resource)
{
    Key key = Unknown;

    if (resource->hasProperty(customKeyProperty))
    {
        bool ok = true;
        key = static_cast<Key>(resource->getProperty(customKeyProperty).toInt(&ok, 16));
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

    if (QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>())
    {
        if (!layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull())
            key = VideoWall;
        else if (layout->isShared())
            key = SharedLayout;
        else
            status = layout->locked() ? Locked : Unknown;
    }
    else
    {
        switch (resource->getStatus())
        {
        case Qn::Online:
            if (key == Server && resource->getId() == qnCommon->remoteGUID())
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
        }
    }

    if (flags.testFlag(Qn::read_only))
        status |= ReadOnly;

    return Key(key | status);
}
