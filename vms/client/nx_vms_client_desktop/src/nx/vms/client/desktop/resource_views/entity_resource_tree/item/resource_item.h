// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>
#include <core/resource/resource.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class ResourceItem: public entity_item_model::AbstractItem
{
    using base_type = entity_item_model::AbstractItem;
public:
    ResourceItem(const QnResourcePtr& resource);

    virtual QVariant data(int role) const override;
    virtual Qt::ItemFlags flags() const override;

private:
    QnResource* resource() const;
    bool isCamera() const;
    bool isLayout() const;
    bool isUser() const;

    QVariant displayData() const;
    QVariant resourceStatusData() const;
    QVariant resourceIconKeyData() const;
    QVariant resourceExtraStatusData() const;
    QVariant resourceExtraInfoData() const;
    QVariant helpTopic() const;
    QVariant parentResourceData() const;

    QVariant cameraGroupIdData() const;
    void initCameraGroupIdNotifications();

    QVariant globalPermissionsData() const;

    QVariant customGroupIdData() const;
    void initCustomGroupIdNotifications();

    void discardCache(QVariant& cachedValue, const QVector<int>& relevantRoles) const;

private:
    QnResourcePtr m_resource;
    const bool m_isCamera = false;
    const bool m_isLayout = false;
    const bool m_isUser = false;

    mutable std::once_flag m_displayFlag;
    mutable QVariant m_displayCache;

    mutable std::once_flag m_statusFlag;
    mutable QVariant m_statusCache;

    mutable std::once_flag m_resourceExtraStatusFlag;
    mutable QVariant m_resourceExtraStatusCache;

    mutable std::once_flag m_resourceIconKeyFlag;
    mutable QVariant m_resourceIconKeyCache;

    mutable std::once_flag m_resourceExtraInfoFlag;
    mutable QVariant m_resourceExtraInfoCache;

    mutable std::once_flag m_parentResourceFlag;
    mutable QVariant m_parentResourceCache;

    mutable QVariant m_cameraGroupIdCache;

    mutable std::once_flag m_globalPermissionsFlag;
    mutable QVariant m_globalPermissionsCache;

    mutable QVariant m_customGroupIdCache;

    const QVariant m_helpTopic;

    mutable nx::utils::ScopedConnections m_connectionsGuard;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
