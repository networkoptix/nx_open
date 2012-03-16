#ifndef QN_RESOURCE_ICON_CACHE_H
#define QN_RESOURCE_ICON_CACHE_H

#include <QObject>
#include <QIcon>
#include <core/resource/resource.h>

/**
 * Cache for resource icons with overlaid status.
 * 
 * Note that this class is not thread-safe.
 */
class QnResourceIconCache: public QObject {
    Q_OBJECT;
    Q_FLAGS(Key KeyPart);

public:
    enum KeyPart {
        Unknown,
        Local,
        Server,
        Servers,
        Layout,
        Camera,
        Image,
        Media,
        User,
        Users,
        TypeMask = 0xFF,

        Offline = (QnResource::Offline + 1) << 8,
        Unauthorized = (QnResource::Unauthorized + 1) << 8,
        Online = (QnResource::Online + 1) << 8,
        StatusMask = 0xFF00
    };
    Q_DECLARE_FLAGS(Key, KeyPart);

    QnResourceIconCache(QObject *parent = NULL);

    virtual ~QnResourceIconCache();

    QIcon icon(QnResource::Flags flags, QnResource::Status status);

    QIcon icon(Key key);

    static Key key(QnResource::Flags flags, QnResource::Status status);

    static QnResourceIconCache *instance();

private:
    QHash<Key, QIcon> m_cache;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceIconCache::Key);

#define qnResIconCache QnResourceIconCache::instance()

#endif // QN_RESOURCE_ICON_CACHE_H
