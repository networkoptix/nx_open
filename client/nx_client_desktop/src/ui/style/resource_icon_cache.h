#ifndef QN_RESOURCE_ICON_CACHE_H
#define QN_RESOURCE_ICON_CACHE_H

#include <QtCore/QObject>
#include <QtGui/QIcon>

#include <core/resource/resource_fwd.h>

/**
 * Cache for resource icons with overlaid status.
 *
 * Note that this class is not thread-safe.
 */
class QnResourceIconCache: public QObject
{
    Q_OBJECT;
    Q_FLAGS(Key KeyPart);

public:
    enum KeyPart
    {
        Unknown,
        LocalResources,
        CurrentSystem,

        Server,
        Servers,
        HealthMonitor,

        Layout,
        SharedLayout,
        Layouts,
        SharedLayouts,
        LayoutTour,
        LayoutTours,

        Camera,
        WearableCamera,
        Cameras,

        Recorder,
        Image,
        Media,
        User,
        Users,
        VideoWall,
        VideoWallItem,
        VideoWallMatrix,
        OtherSystem,
        OtherSystems,
        IOModule,
        WebPage,
        WebPages,

        TypeMask        = 0xFF,

        Offline         = 1 << 8,
        Unauthorized    = 2 << 8,
        Online          = 3 << 8,
        Locked          = 4 << 8,
        Incompatible    = 5 << 8,
        Control         = 6 << 8,
        StatusMask      = 0xFF00,

        /* Additional flags */
        ReadOnly        = 0x10000
    };
    Q_DECLARE_FLAGS(Key, KeyPart)

    QnResourceIconCache(QObject* parent = nullptr);

    virtual ~QnResourceIconCache();

    QIcon icon(const QnResourcePtr& resource);

    /**
     * @brief icon
     * @param key
     * @param unchecked         Do not check against TypeMask
     * @return
     */
    QIcon icon(Key key, bool unchecked = false);

    static Key key(const QnResourcePtr& resource);

    /**
     * @brief setKey            Bind custom key to the provided resource instance.
     * @param resource          Target resource.
     * @param key               Custom key.
     */
    static void setKey(const QnResourcePtr& resource, Key key);
    static void clearKey(const QnResourcePtr& resource);

    static QnResourceIconCache* instance();


private:
    QHash<Key, QIcon> m_cache;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceIconCache::Key)

#define qnResIconCache QnResourceIconCache::instance()

#endif // QN_RESOURCE_ICON_CACHE_H
