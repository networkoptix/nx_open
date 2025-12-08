// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "core_resource_item.h"

#include <QtCore/QVariant>

#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/api/data/resource_property_key.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/resource_views/data/resource_extra_status.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/core/skin/resource_icon_cache.h>
#include <nx/vms/common/system_context.h>

namespace nx::vms::client::core {
namespace entity_resource_tree {

CoreResourceItem::CoreResourceItem(const QnResourcePtr& resource):
    base_type(),
    m_resource(resource),
    m_isCamera(m_resource.dynamicCast<QnVirtualCameraResource>()),
    m_isLayout(m_resource.dynamicCast<QnLayoutResource>()),
    m_isUser(m_resource->hasFlags(Qn::user))
{
    m_connectionsGuard.add(m_resource->connect(m_resource.get(), &QnResource::flagsChanged,
        [this]
        {
            if (m_resource->hasFlags(Qn::removed))
                m_connectionsGuard.reset();
            else
                notifyDataChanged({core::ResourceFlagsRole});
        }));

    initCameraGroupIdNotifications();
    initCustomGroupIdNotifications();
}

QVariant CoreResourceItem::data(int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::EditRole:
            return displayData();

        case core::ResourceRole:
            return QVariant::fromValue(m_resource);

        case core::ResourceFlagsRole:
            return QVariant::fromValue(m_resource->flags());

        case core::ResourceStatusRole:
            return resourceStatusData();

        case core::ResourceExtraStatusRole:
            return resourceExtraStatusData();

        case core::ParentResourceRole:
            return parentResourceData();

        case core::CameraGroupIdRole:
            return cameraGroupIdData();

        case core::ResourceTreeCustomGroupIdRole:
            return customGroupIdData();

        case core::ExtraInfoRole:
            return resourceExtraInfoData();

        case core::ResourceIconKeyRole:
            return resourceIconKeyData();
    }
    return QVariant();
}

Qt::ItemFlags CoreResourceItem::flags() const
{
    return {Qt::ItemIsEnabled, Qt::ItemIsSelectable};
}

QnResource* CoreResourceItem::resource() const
{
    return m_resource.get();
}

bool CoreResourceItem::isCamera() const
{
    return m_isCamera;
}

bool CoreResourceItem::isLayout() const
{
    return m_isLayout;
}

bool CoreResourceItem::isUser() const
{
    return m_isUser;
}

QVariant CoreResourceItem::displayData() const
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

QVariant CoreResourceItem::resourceStatusData() const
{
    const auto initNotifications =
        [this]
        {
            m_connectionsGuard.add(resource()->connect(resource(), &QnResource::statusChanged,
                [this] { discardCache(m_statusCache, {core::ResourceStatusRole}); }));
        };

    std::call_once(m_statusFlag, initNotifications);

    if (m_statusCache.isNull())
        m_statusCache = QVariant::fromValue(m_resource->getStatus());

    return m_statusCache;
}

QVariant CoreResourceItem::resourceExtraStatusData() const
{
    if (!isCamera() && !isLayout())
        return QVariant();

    const auto initNotifications =
        [this]
        {
            const auto discardExtraStatusCache =
                [this] { discardCache(m_resourceExtraStatusCache, {core::ResourceExtraStatusRole}); };

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
                        [this, cameraId = camera->getId()](const QnVirtualCameraResourcePtr& securityCamera)
                        {
                            if (securityCamera->getId() == cameraId)
                                discardCache(m_resourceExtraStatusCache, {core::ResourceExtraStatusRole});
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

QVariant CoreResourceItem::resourceExtraInfoData() const
{
    const auto initNotifications =
        [this]
        {
            const auto discardExtraInfo =
                [this] { discardCache(m_resourceExtraInfoCache, {core::ExtraInfoRole}); };

            if (auto userResource = m_resource.objectCast<QnUserResource>())
            {
                m_connectionsGuard.add(userResource->connect(userResource.get(),
                    &QnUserResource::userGroupsChanged, discardExtraInfo));

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

QVariant CoreResourceItem::cameraGroupIdData() const
{
    if (!isCamera())
        return QVariant();

    if (m_cameraGroupIdCache.isNull())
        m_cameraGroupIdCache = m_resource.staticCast<QnVirtualCameraResource>()->getGroupId();

    return m_cameraGroupIdCache;
}

void CoreResourceItem::initCameraGroupIdNotifications()
{
    if (!isCamera())
        return;

    const auto camera = m_resource.staticCast<QnVirtualCameraResource>().get();

    m_connectionsGuard.add(camera->connect(camera, &QnVirtualCameraResource::groupIdChanged,
        [this] { discardCache(m_cameraGroupIdCache, {core::CameraGroupIdRole}); }));
}

QVariant CoreResourceItem::parentResourceData() const
{
    if (!isCamera())
        return QVariant();

    const auto initNotifications =
        [this]
        {
            m_connectionsGuard.add(m_resource->connect(m_resource.get(), &QnResource::parentIdChanged,
                [this] { discardCache(m_parentResourceCache, {core::ParentResourceRole}); }));
        };

    std::call_once(m_parentResourceFlag, initNotifications);

    if (m_parentResourceCache.isNull())
        m_parentResourceCache = QVariant::fromValue<QnResourcePtr>(m_resource->getParentResource());

    return m_parentResourceCache;
}

QVariant CoreResourceItem::customGroupIdData() const
{
    if (!isCamera() && !m_resource->hasFlags(Qn::web_page) && !m_resource->hasFlags(Qn::layout))
        return QVariant();

    if (m_customGroupIdCache.isNull())
    {
        m_customGroupIdCache =
            core::entity_resource_tree::resource_grouping::resourceCustomGroupId(m_resource);
    }

    return m_customGroupIdCache;
}

void CoreResourceItem::initCustomGroupIdNotifications()
{
    m_connectionsGuard.add(m_resource->connect(m_resource.get(), &QnResource::propertyChanged,
        [this](const QnResourcePtr&, const QString& key)
        {
            if (key == api::resource_properties::kCustomGroupIdPropertyKey)
                discardCache(m_customGroupIdCache, {core::ResourceTreeCustomGroupIdRole});
        }));
}

void CoreResourceItem::initResourceIconKeyNotifications() const
{
    const auto discardIconCache =
        [this] { discardCache(m_resourceIconKeyCache, {core::ResourceIconKeyRole}); };

    m_connectionsGuard.add(resource()->connect(resource(), &QnResource::flagsChanged,
        discardIconCache));

    m_connectionsGuard.add(resource()->connect(resource(), &QnResource::statusChanged,
        discardIconCache));

    if (isCamera())
    {
        const auto camera = m_resource.staticCast<QnVirtualCameraResource>().get();
        m_connectionsGuard.add(camera->connect(
            camera,
            &QnVirtualCameraResource::statusFlagsChanged,
            discardIconCache));
    }

    const auto layout = m_resource.objectCast<core::LayoutResource>();
    if (layout && !m_resource->hasFlags(Qn::exported_layout))
    {
        m_connectionsGuard.add(layout->connect(
            layout.get(), &core::LayoutResource::layoutTypeChanged, discardIconCache));
    }

    if (m_resource.objectCast<QnWebPageResource>())
    {
        m_connectionsGuard.add(
            resource()->connect(m_resource.get(), &QnResource::parentIdChanged,
                discardIconCache));

        m_connectionsGuard.add(
            resource()->connect(m_resource.get(), &QnResource::propertyChanged,
                discardIconCache));
    }
}

QVariant CoreResourceItem::resourceIconKeyData() const
{
    std::call_once(
        m_resourceIconKeyFlag, &CoreResourceItem::initResourceIconKeyNotifications, this);

    if (m_resourceIconKeyCache.isNull())
        m_resourceIconKeyCache = static_cast<int>(appContext()->resourceIconCache()->key(m_resource));

    return m_resourceIconKeyCache;
}

void CoreResourceItem::discardCache(QVariant& cachedValue, const QVector<int>& relevantRoles) const
{
    cachedValue.clear();
    notifyDataChanged(relevantRoles);
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::core
