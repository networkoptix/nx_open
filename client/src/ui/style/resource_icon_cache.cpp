#include "resource_icon_cache.h"
#include <QPixmap>
#include <QPainter>
#include "skin.h"

Q_GLOBAL_STATIC(QnResourceIconCache, qn_resourceIconCache);

QnResourceIconCache::QnResourceIconCache(QObject *parent): QObject(parent) {
    m_cache.insert(Unknown,                 QIcon());
    m_cache.insert(Local,                   qnSkin->icon("home.png"));
    m_cache.insert(Server,                  qnSkin->icon("server.png"));
    m_cache.insert(Servers,                 qnSkin->icon("servers.png"));
    m_cache.insert(Layout,                  qnSkin->icon("layout.png"));
    m_cache.insert(Camera,                  qnSkin->icon("camera.png"));
    m_cache.insert(Image,                   qnSkin->icon("snapshot.png"));
    m_cache.insert(Media,                   qnSkin->icon("media.png"));
    m_cache.insert(User,                    qnSkin->icon("user.png"));
    m_cache.insert(Users,                   qnSkin->icon("users.png"));

    m_cache.insert(Server | Offline,        qnSkin->icon("server_offline.png"));
    m_cache.insert(Camera | Offline,        qnSkin->icon("camera_offline.png"));
#if 0
    m_cache.insert(User | Offline,          qnSkin->icon("user_offline.png"));
#endif

    m_cache.insert(Camera | Unauthorized,   qnSkin->icon("camera_unauthorized.png"));

#if 0
    m_cache.insert(Offline,                 Skin::icon("offline.png"));
    m_cache.insert(Unauthorized,            Skin::icon("unauthorized.png"));
#endif
}

QnResourceIconCache::~QnResourceIconCache() {
    return;
}

QnResourceIconCache *QnResourceIconCache::instance() {
    return qn_resourceIconCache();
}

QIcon QnResourceIconCache::icon(Key key) {
    /* This function will be called from GUI thread only, 
     * so no synchronization is needed. */

    if((key & TypeMask) == Unknown)
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
            r.setTopLeft(r.center());
            overlay.paint(&painter, r, Qt::AlignRight | Qt::AlignBottom);
        }
        icon = QIcon(pixmap);
    }

    m_cache.insert(key, icon);
    return icon;
}

QIcon QnResourceIconCache::icon(QnResource::Flags flags, QnResource::Status status) {
    return icon(key(flags, status));
}

QnResourceIconCache::Key QnResourceIconCache::key(QnResource::Flags flags, QnResource::Status status) {
    Key key = Unknown;

    const QnResource::Flags localServer = QnResource::local | QnResource::server;

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
    }

    return Key(key | ((status + 1) << 8));
}

