#include "resource_icon_cache.h"

#include <QtGui/QPixmap>
#include <QtGui/QPainter>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>

#include <ui/style/skin.h>

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
    m_cache.insert(Recorder,                qnSkin->icon("tree/recorder.png"));
    m_cache.insert(Image,                   qnSkin->icon("tree/snapshot.png"));
    m_cache.insert(Media,                   qnSkin->icon("tree/media.png"));
    m_cache.insert(User,                    qnSkin->icon("tree/user.png"));
    m_cache.insert(Users,                   qnSkin->icon("tree/users.png"));
    m_cache.insert(VideoWall,               qnSkin->icon("tree/videowall.png"));
    m_cache.insert(VideoWallItem,           qnSkin->icon("tree/screen.png"));
    m_cache.insert(VideoWallMatrix,         qnSkin->icon("tree/matrix.png"));

    m_cache.insert(Server | Offline,        qnSkin->icon("tree/server_offline.png"));
    m_cache.insert(Camera | Offline,        qnSkin->icon("tree/camera_offline.png"));
    m_cache.insert(Camera | Unauthorized,   qnSkin->icon("tree/camera_unauthorized.png"));
    m_cache.insert(Layout | Locked,         qnSkin->icon("tree/layout_locked.png"));

    m_cache.insert(Offline,                 qnSkin->icon("tree/offline.png"));
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

    QnResource::Flags flags = resource->flags();
    if ((flags & QnResource::local_server) == QnResource::local_server) {
        key = Local;
    } else if ((flags & QnResource::server) == QnResource::server) {
        key = Server;
    } else if ((flags & QnResource::layout) == QnResource::layout) {
        key = Layout;
    } else if ((flags & QnResource::live_cam) == QnResource::live_cam) {
        key = Camera;
    } else if ((flags & QnResource::SINGLE_SHOT) == QnResource::SINGLE_SHOT) {
        key = Image;
    } else if ((flags & QnResource::ARCHIVE) == QnResource::ARCHIVE) {
        key = Media;
    } else if ((flags & QnResource::server_archive) == QnResource::server_archive) {
        key = Media;
    } else if ((flags & QnResource::user) == QnResource::user) {
        key = User;
    } else if ((flags & QnResource::videowall) == QnResource::videowall) {
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
        case QnResource::Online:
            status = Online;
            break;
        case QnResource::Offline:
            status = Offline;
            break;
        case QnResource::Unauthorized:
            status = Unauthorized;
            break;
        default:
            break;
        }
    }

    return Key(key | status);
}
