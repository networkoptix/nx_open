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

namespace {
    const QString customKeyProperty = lit("customIconCacheKey");
}

QnResourceIconCache::QnResourceIconCache(QObject *parent): QObject(parent) {
    m_cache.insert(Unknown,                 QIcon());
    m_cache.insert(Local,                   qnSkin->icon("tree/home.png"));
    m_cache.insert(Server,                  qnSkin->icon("tree/server.png"));
    m_cache.insert(Servers,                 qnSkin->icon("tree/servers.png"));
    m_cache.insert(Layout,                  qnSkin->icon("tree/layout.png"));
    m_cache.insert(Camera,                  qnSkin->icon("tree/camera.png"));
    m_cache.insert(IOModule,                qnSkin->icon("tree/io.png"));
    m_cache.insert(Recorder,                qnSkin->icon("tree/recorder.png"));
    m_cache.insert(Image,                   qnSkin->icon("tree/snapshot.png"));
    m_cache.insert(Media,                   qnSkin->icon("tree/media.png"));
    m_cache.insert(User,                    qnSkin->icon("tree/user.png"));
    m_cache.insert(Users,                   qnSkin->icon("tree/users.png"));
    m_cache.insert(VideoWall,               qnSkin->icon("tree/videowall.png"));
    m_cache.insert(VideoWallItem,           qnSkin->icon("tree/screen.png"));
    m_cache.insert(VideoWallMatrix,         qnSkin->icon("tree/matrix.png"));
    m_cache.insert(OtherSystem,             qnSkin->icon("tree/system.png"));
    m_cache.insert(OtherSystems,            qnSkin->icon("tree/other_systems.png"));

    m_cache.insert(Media | Offline,         qnSkin->icon("tree/media_offline.png"));
    m_cache.insert(Image | Offline,         qnSkin->icon("tree/snapshot_offline.png"));
    m_cache.insert(Server | Offline,        qnSkin->icon("tree/server_offline.png"));
    m_cache.insert(Server | Incompatible,   qnSkin->icon("tree/server_incompatible.png"));
    m_cache.insert(Server | Control,        qnSkin->icon("tree/server_current.png"));
    m_cache.insert(Server | Unauthorized,   qnSkin->icon("tree/server_unauthorized.png"));
    m_cache.insert(Camera | Offline,        qnSkin->icon("tree/camera_offline.png"));
    m_cache.insert(Camera | Unauthorized,   qnSkin->icon("tree/camera_unauthorized.png"));
    m_cache.insert(Layout | Locked,         qnSkin->icon("tree/layout_locked.png"));
    m_cache.insert(VideoWallItem | Locked,  qnSkin->icon("tree/screen_locked.png"));
    m_cache.insert(VideoWallItem | Control, qnSkin->icon("tree/screen_controlled.png"));
    m_cache.insert(VideoWallItem | Offline, qnSkin->icon("tree/screen_offline.png"));
    m_cache.insert(IOModule | Offline,      qnSkin->icon("tree/io_offline.png"));
    m_cache.insert(IOModule | Unauthorized, qnSkin->icon("tree/io_unauthorized.png"));

    /* Read-only server that is auto-discovered. */
    m_cache.insert(Server | Incompatible | ReadOnly,    qnSkin->icon("tree/server_incompatible_readonly.png"));

    /* Read-only server we are connected to. */
    m_cache.insert(Server | Control | ReadOnly,         qnSkin->icon("tree/server_readonly.png"));

    /* Overlays. Should not be used. */
    m_cache.insert(Offline,                 qnSkin->icon("tree/offline.png"));
    m_cache.insert(Unauthorized,            qnSkin->icon("tree/unauthorized.png"));
}

QnResourceIconCache::~QnResourceIconCache() {
    return;
}

QnResourceIconCache *QnResourceIconCache::instance() {
    return qn_resourceIconCache();
}

QIcon QnResourceIconCache::icon(Key key, bool unchecked) {
    /* This function will be called from GUI thread only, 
     * so no synchronization is needed. */

    if((key & TypeMask) == Unknown && !unchecked)
        key = Unknown;

    if(m_cache.contains(key))
        return m_cache.value(key);
    
    QIcon icon = m_cache.value(key & TypeMask);
    QIcon overlay = m_cache.value(key & StatusMask);
    if(!icon.isNull() && !overlay.isNull()) {
        Q_ASSERT_X(false, Q_FUNC_INFO, "All icons should be pre-generated.");

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

QIcon QnResourceIconCache::icon(const QnResourcePtr &resource) {
    if (!resource)
        return QIcon();
    return icon(key(resource));
}

void QnResourceIconCache::setKey(QnResourcePtr &resource, Key key) {
    resource->setProperty(customKeyProperty, QString::number(static_cast<int>(key), 16));
}

QnResourceIconCache::Key QnResourceIconCache::key(const QnResourcePtr &resource) {
    Key key = Unknown;

    if (resource->hasProperty(customKeyProperty)) {
        bool ok = true;
        key = static_cast<Key>(resource->getProperty(customKeyProperty).toInt(&ok, 16));
        if (ok)
            return key;
    }

    Qn::ResourceFlags flags = resource->flags();
    if ((flags & Qn::local_server) == Qn::local_server) {
        key = Local;
    } else if ((flags & Qn::server) == Qn::server) {
        key = Server;
    } else if ((flags & Qn::layout) == Qn::layout) {
        key = Layout;
    } else if ((flags & Qn::live_cam) == Qn::live_cam) {
        if (flags & Qn::io_module)
            key = IOModule;
        else
            key = Camera;
    } else if ((flags & Qn::SINGLE_SHOT) == Qn::SINGLE_SHOT) {
        key = Image;
    } else if ((flags & Qn::ARCHIVE) == Qn::ARCHIVE) {
        key = Media;
    } else if ((flags & Qn::server_archive) == Qn::server_archive) {
        key = Media;
    } else if ((flags & Qn::user) == Qn::user) {
        key = User;
    } else if ((flags & Qn::videowall) == Qn::videowall) {
        key = VideoWall;
    } 

    Key status = Unknown;
    
    if (QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>()) {
        if (!layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull())
            key = VideoWall;
        else
            status = layout->locked() ? Locked : Unknown;
    }
    else {
        switch (resource->getStatus()) {
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
