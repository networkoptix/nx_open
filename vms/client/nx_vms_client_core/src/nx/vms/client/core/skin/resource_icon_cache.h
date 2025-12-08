// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtGui/QIcon>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/resource_views/data/resource_extra_status.h>
#include <nx/vms/client/core/skin/skin.h>

namespace nx::vms::client::core {

/**
 * Cache for resource icons with overlaid status.
 *
 * Note that this class is not thread-safe.
 */
class NX_VMS_CLIENT_CORE_API ResourceIconCache: public QObject
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
        HealthMonitors,

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
        OrganizationUser,
        ChannelPartnerUser,
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
        AlwaysSelected  = 0x20000,
        NotificationMode = 0x30000,
        AdditionalFlagsMask = 0xFF0000
    };
    Q_DECLARE_FLAGS(Key, KeyPart)
    Q_FLAG(Key)


    class IconWithDescription
    {
    public:
        IconWithDescription() = default;
        IconWithDescription(const ColorizedIconDeclaration& iconDescription);

        QIcon icon;
        ColorizedIconDeclaration iconDescription;
    };

    ResourceIconCache(QObject* parent = nullptr);
    virtual ~ResourceIconCache();

    QIcon icon(const QnResourcePtr& resource);

    /**
     * @brief icon
     * @param key
     * @return
     */
    QIcon icon(Key key);
    QPixmap iconPixmap(Key key,
        const QSize& requestedSize,
        const SvgIconColorer::ThemeSubstitutions& svgColorSubstitutions,
        QnIcon::Mode mode = QnIcon::Normal);

    virtual QString iconPath(Key key) const;
    QString iconPath(const QnResourcePtr& resource) const;
    QColor iconColor(Key key, QnIcon::Mode mode = QnIcon::Normal) const;
    QColor iconColor(const QnResourcePtr& resource) const;

    Key userKey(const QnUserResourcePtr& user) const;
    virtual Key key(const QnResourcePtr& resource) const;
    QIcon cameraRecordingStatusIcon(ResourceExtraStatus status) const;

protected:
    static QIcon loadIcon(const QString& name,
        const SvgIconColorer::ThemeSubstitutions& themeSubstitutions = {});
    static SvgIconColorer::ThemeSubstitutions treeThemeSubstitutions();
    static SvgIconColorer::ThemeSubstitutions treeThemeOfflineSubstitutions();

    IconWithDescription searchIconOutsideCache(Key key);
    IconWithDescription iconDescription(Key key);

    QHash<Key, IconWithDescription> m_cache;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ResourceIconCache::Key)

} // namespace nx::vms::client::core
