// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_RESOURCE_ICON_CACHE_H
#define QN_RESOURCE_ICON_CACHE_H

#include <QtCore/QObject>
#include <QtGui/QIcon>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource_views/data/resource_extra_status.h>

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
        ExportedLayout,
        ExportedEncryptedLayout,
        SharedLayout,
        CloudLayout,
        IntercomLayout,
        Layouts,
        SharedLayouts,
        Showreel,
        Showreels,

        Camera,
        VirtualCamera,
        CrossSystemStatus,
        Cameras,

        Recorder,
        MultisensorCamera,

        Image,
        Media,
        User,
        LocalUser,
        CloudUser,
        LdapUser,
        TemporaryUser,
        Users,
        VideoWall,
        VideoWallItem,
        VideoWallMatrix,
        OtherSystem,
        OtherSystems,
        IOModule,
        Integration,
        Integrations,
        WebPage,
        WebPages,
        AnalyticsEngine,
        AnalyticsEngines,

        Client,

        CloudSystem,

        IntegrationProxied,
        WebPageProxied,

        TypeMask        = 0xFF,

        Offline = 1 << 8,
        Unauthorized = 2 << 8,
        Online = 3 << 8,
        Locked = 4 << 8,
        Incompatible = 5 << 8,
        Control = 6 << 8,
        MismatchedCertificate = 7 << 8,
        StatusMask = 0xFF00,

        /* Additional flags */
        ReadOnly        = 0x10000,
        AlwaysSelected  = 0x20000
    };
    Q_DECLARE_FLAGS(Key, KeyPart)
    Q_FLAG(Key)

    QnResourceIconCache(QObject* parent = nullptr);

    virtual ~QnResourceIconCache();

    QIcon icon(const QnResourcePtr& resource);

    /**
     * @brief icon
     * @param key
     * @return
     */
    QIcon icon(Key key);

    QString iconPath(Key key) const;
    QString iconPath(const QnResourcePtr& resource) const;

    static Key userKey(const QnUserResourcePtr& user);

    static Key key(const QnResourcePtr& resource);

    static QnResourceIconCache* instance();

    static QIcon cameraRecordingStatusIcon(nx::vms::client::desktop::ResourceExtraStatus status);

private:
    QHash<Key, QString> m_paths;
    QHash<Key, QIcon> m_cache;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceIconCache::Key)

#define qnResIconCache QnResourceIconCache::instance()

#endif // QN_RESOURCE_ICON_CACHE_H
