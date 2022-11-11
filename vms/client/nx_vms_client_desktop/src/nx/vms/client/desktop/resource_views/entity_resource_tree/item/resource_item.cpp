// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_item.h"

#include <QtCore/QVariant>

#include <client/client_globals.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/client/desktop/resource_views/data/resource_extra_status.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/common/system_context.h>
#include <ui/help/help_topics.h>

namespace {

QVariant resourceHelpTopic(const QnResourcePtr& resource)
{
    if (resource.isNull())
        return QVariant();

    if (resource->hasFlags(Qn::fake_server))
        return Qn::OtherSystems_Help;

    if (resource->hasFlags(Qn::server))
        return Qn::MainWindow_Tree_Servers_Help;

    if (resource->hasFlags(Qn::io_module))
        return Qn::IOModules_Help;

    if (resource->hasFlags(Qn::live_cam))
        return Qn::MainWindow_Tree_Camera_Help;

    if (resource->hasFlags(Qn::layout))
    {
        auto layout = resource.staticCast<QnLayoutResource>();
        if (layout->isFile())
            return Qn::MainWindow_Tree_MultiVideo_Help;
        return Qn::MainWindow_Tree_Layouts_Help;
    }

    if (resource->hasFlags(Qn::user))
        return Qn::MainWindow_Tree_Users_Help;

    if (resource->hasFlags(Qn::web_page))
        return Qn::MainWindow_Tree_WebPage_Help;

    if (resource->hasFlags(Qn::videowall))
        return Qn::Videowall_Help;

    if (resource->hasFlags(Qn::local))
        return Qn::MainWindow_Tree_Exported_Help;

    return QVariant();
}

} // namespace

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

ResourceItem::ResourceItem(const QnResourcePtr& resource):
    base_type(),
    m_resource(resource),
    m_isCamera(m_resource.dynamicCast<QnVirtualCameraResource>()),
    m_isLayout(m_resource.dynamicCast<QnLayoutResource>()),
    m_isUser(m_resource->hasFlags(Qn::user)),
    m_helpTopic(resourceHelpTopic(resource))
{
    m_connectionsGuard.add(m_resource->connect(m_resource.get(), &QnResource::flagsChanged,
        [this]
        {
            if (m_resource->hasFlags(Qn::removed))
                m_connectionsGuard.reset();
            else
                notifyDataChanged({Qn::ResourceFlagsRole});
        }));

    // Item will not send notifications for the never accessed data roles. This workaround is
    // needed to guarantee that all notifications for the roles listed below will be delivered.
    // TODO: #vbreus Roles that trigger model layout changes should be declared in a special way.
    data(Qn::GlobalPermissionsRole);
    initCameraGroupIdNotifications();
    initCustomGroupIdNotifications();
}

QVariant ResourceItem::data(int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::EditRole:
            return displayData();

        case Qn::ResourceIconKeyRole:
            return resourceIconKeyData();

        case Qn::ResourceRole:
            return QVariant::fromValue(m_resource);

        case Qn::ResourceFlagsRole:
            return QVariant::fromValue(m_resource->flags());

        case Qn::ResourceStatusRole:
            return resourceStatusData();

        case Qn::ResourceExtraStatusRole:
            return resourceExtraStatusData();

        case Qn::HelpTopicIdRole:
            return helpTopic();

        case Qn::ParentResourceRole:
            return parentResourceData();

        case Qn::CameraGroupIdRole:
            return cameraGroupIdData();

        case Qn::GlobalPermissionsRole:
            return globalPermissionsData();

        case Qn::ResourceTreeCustomGroupIdRole:
            return customGroupIdData();

        case Qn::ExtraInfoRole:
            return resourceExtraInfoData();
    }
    return QVariant();
}

Qt::ItemFlags ResourceItem::flags() const
{
    return {Qt::ItemIsEnabled, Qt::ItemIsSelectable};
}

QnResource* ResourceItem::resource() const
{
    return m_resource.get();
}

bool ResourceItem::isCamera() const
{
    return m_isCamera;
}

bool ResourceItem::isLayout() const
{
    return m_isLayout;
}

bool ResourceItem::isUser() const
{
    return m_isUser;
}

QVariant ResourceItem::displayData() const
{
    const auto initNotifications =
        [this]
        {
            m_connectionsGuard.add(resource()->connect(resource(), &QnResource::nameChanged,
                [this] { discardCache(m_displayCache, {Qt::DisplayRole, Qt::ToolTipRole}); }));
        };

    std::call_once(m_displayFlag, initNotifications);

    if (m_displayCache.isNull())
        m_displayCache = m_resource->getName();

    return m_displayCache;
}

QVariant ResourceItem::resourceStatusData() const
{
    const auto initNotifications =
        [this]
        {
            m_connectionsGuard.add(resource()->connect(resource(), &QnResource::statusChanged,
                [this] { discardCache(m_statusCache, {Qn::ResourceStatusRole}); }));
        };

    std::call_once(m_statusFlag, initNotifications);

    if (m_statusCache.isNull())
        m_statusCache = QVariant::fromValue(m_resource->getStatus());

    return m_statusCache;
}

QVariant ResourceItem::resourceIconKeyData() const
{
    const auto initNotifications =
        [this]
        {
            const auto discardIconCache =
                [this] { discardCache(m_resourceIconKeyCache, {Qn::ResourceIconKeyRole}); };

            m_connectionsGuard.add(resource()->connect(resource(), &QnResource::flagsChanged,
                discardIconCache));

            m_connectionsGuard.add(resource()->connect(resource(), &QnResource::statusChanged,
                discardIconCache));

            if (m_isCamera)
            {
                const auto camera = m_resource.staticCast<QnVirtualCameraResource>().get();
                m_connectionsGuard.add(camera->connect(
                    camera,
                    &QnVirtualCameraResource::statusFlagsChanged,
                    discardIconCache));
            }

            const bool nonExportedLayout = m_resource->hasFlags(Qn::layout)
                && !m_resource->hasFlags(Qn::exported_layout);

            if (nonExportedLayout || m_resource->hasFlags(Qn::web_page))
            {
                const auto layout = m_resource.staticCast<QnLayoutResource>().get();

                m_connectionsGuard.add(layout->connect(layout, &QnResource::parentIdChanged,
                    discardIconCache));

                m_connectionsGuard.add(layout->connect(layout, &QnLayoutResource::lockedChanged,
                    discardIconCache));
            }

            if (m_resource->hasFlags(Qn::web_page))
            {
                m_connectionsGuard.add(
                    m_resource->connect(m_resource.get(), &QnResource::propertyChanged,
                    [this] { discardCache(m_resourceIconKeyCache, {Qn::ResourceIconKeyRole}); }));
            }
        };

    std::call_once(m_resourceIconKeyFlag, initNotifications);

    if (m_resourceIconKeyCache.isNull())
        m_resourceIconKeyCache = static_cast<int>(QnResourceIconCache::key(m_resource));

    return m_resourceIconKeyCache;
}

QVariant ResourceItem::resourceExtraStatusData() const
{
    if (!isCamera() && !isLayout())
        return QVariant();

    const auto initNotifications =
        [this]
        {
            const auto discardExtraStatusCache =
                [this] { discardCache(m_resourceExtraStatusCache, {Qn::ResourceExtraStatusRole}); };

            if (m_isLayout)
            {
                const auto layout = m_resource.staticCast<QnLayoutResource>().get();
                m_connectionsGuard.add(QObject::connect(
                    layout, &QnLayoutResource::lockedChanged, discardExtraStatusCache));
            }
            else if (m_isCamera)
            {
                const auto camera = m_resource.staticCast<QnVirtualCameraResource>().get();
                m_connectionsGuard.add(QObject::connect(
                    camera, &QnVirtualCameraResource::statusChanged, discardExtraStatusCache));

                m_connectionsGuard.add(QObject::connect(
                    camera, &QnVirtualCameraResource::scheduleEnabledChanged, discardExtraStatusCache));

                m_connectionsGuard.add(QObject::connect(
                    camera, &QnVirtualCameraResource::statusFlagsChanged, discardExtraStatusCache));

                const auto context = camera->systemContext();
                if (!context)
                    return;

                const auto historyPool = context->cameraHistoryPool();
                m_connectionsGuard.add(
                    QObject::connect(historyPool, &QnCameraHistoryPool::cameraFootageChanged,
                        [this, cameraId = camera->getId()](const QnSecurityCamResourcePtr& securityCamera)
                        {
                            if (securityCamera->getId() == cameraId)
                                discardCache(m_resourceExtraStatusCache, {Qn::ResourceExtraStatusRole});
                        }));
            }
            else
            {
                NX_ASSERT("Mustn't be here");
            }
    };

    std::call_once(m_resourceExtraStatusFlag, initNotifications);

    if (m_resourceExtraStatusCache.isNull())
        m_resourceExtraStatusCache = QVariant::fromValue(getResourceExtraStatus(m_resource));

    return m_resourceExtraStatusCache;
}

QVariant ResourceItem::resourceExtraInfoData() const
{
    const auto initNotifications =
        [this]
        {
            const auto discardExtraInfo =
                [this] { discardCache(m_resourceExtraInfoCache, {Qn::ExtraInfoRole}); };

            if (auto userResource = m_resource.objectCast<QnUserResource>())
            {
                m_connectionsGuard.add(userResource->connect(userResource.get(),
                    &QnUserResource::userRolesChanged, discardExtraInfo));

                m_connectionsGuard.add(userResource->connect(userResource.get(),
                    &QnUserResource::permissionsChanged, discardExtraInfo));
            }
            else
            {
                m_connectionsGuard.add(m_resource->connect(m_resource.get(),
                    &QnResource::urlChanged, discardExtraInfo));
            }
        };

    std::call_once(m_resourceExtraInfoFlag, initNotifications);

    if (m_resourceExtraInfoCache.isNull())
        m_resourceExtraInfoCache = QnResourceDisplayInfo(m_resource).extraInfo();

    return m_resourceExtraInfoCache;
};

QVariant ResourceItem::helpTopic() const
{
    return m_helpTopic;
}

QVariant ResourceItem::cameraGroupIdData() const
{
    if (!isCamera())
        return QVariant();

    if (m_cameraGroupIdCache.isNull())
        m_cameraGroupIdCache = m_resource.staticCast<QnVirtualCameraResource>()->getGroupId();

    return m_cameraGroupIdCache;
}

void ResourceItem::initCameraGroupIdNotifications()
{
    if (!isCamera())
        return;

    const auto camera = m_resource.staticCast<QnVirtualCameraResource>().get();

    m_connectionsGuard.add(camera->connect(camera, &QnVirtualCameraResource::groupIdChanged,
        [this] { discardCache(m_cameraGroupIdCache, {Qn::CameraGroupIdRole}); }));
}

QVariant ResourceItem::parentResourceData() const
{
    if (!isCamera())
        return QVariant();

    const auto initNotifications =
        [this]
        {
            m_connectionsGuard.add(m_resource->connect(m_resource.get(), &QnResource::parentIdChanged,
                [this] { discardCache(m_parentResourceCache, {Qn::ParentResourceRole}); }));
        };

    std::call_once(m_parentResourceFlag, initNotifications);

    if (m_parentResourceCache.isNull())
        m_parentResourceCache = QVariant::fromValue<QnResourcePtr>(m_resource->getParentResource());

    return m_parentResourceCache;
}

QVariant ResourceItem::globalPermissionsData() const
{
    if (!isUser())
        return QVariant();

    const auto initNotifications =
        [this]
        {
            const auto user = m_resource.staticCast<QnUserResource>();
            m_connectionsGuard.add(user->connect(user.get(), &QnUserResource::permissionsChanged,
                [this] { discardCache(m_globalPermissionsCache, {Qn::GlobalPermissionsRole}); }));
        };

    std::call_once(m_parentResourceFlag, initNotifications);

    if (m_globalPermissionsCache.isNull())
    {
        using namespace nx::vms::api;

        const auto user = m_resource.staticCast<QnUserResource>();
        const auto permissionsManager = user->systemContext()->globalPermissionsManager();
        m_globalPermissionsCache =
            QVariant::fromValue<GlobalPermissions>(permissionsManager->globalPermissions(user));
    }

    return m_globalPermissionsCache;
}

QVariant ResourceItem::customGroupIdData() const
{
    if (!isCamera() && !m_resource->hasFlags(Qn::web_page))
        return QVariant();

    if (m_customGroupIdCache.isNull())
        m_customGroupIdCache = resource_grouping::resourceCustomGroupId(m_resource);

    return m_customGroupIdCache;
}

void ResourceItem::initCustomGroupIdNotifications()
{
    m_connectionsGuard.add(m_resource->connect(m_resource.get(), &QnResource::propertyChanged,
        [this](const QnResourcePtr&, const QString& key)
        {
            if (key == resource_grouping::kCustomGroupIdPropertyKey)
                discardCache(m_customGroupIdCache, {Qn::ResourceTreeCustomGroupIdRole});
        }));
}

void ResourceItem::discardCache(QVariant& cachedValue, const QVector<int>& relevantRoles) const
{
    cachedValue.clear();
    notifyDataChanged(relevantRoles);
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
