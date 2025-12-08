// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/resource_views/entity_item_model/item/abstract_item.h>
#include <nx/vms/client/core/skin/resource_icon_cache.h>

namespace nx::vms::client::core {
namespace entity_resource_tree {

class NX_VMS_CLIENT_CORE_API CoreResourceItem: public core::entity_item_model::AbstractItem
{
    using base_type = core::entity_item_model::AbstractItem;
public:
    CoreResourceItem(const QnResourcePtr& resource);

    virtual QVariant data(int role) const override;
    virtual Qt::ItemFlags flags() const override;

protected:
    QnResource* resource() const;
    bool isCamera() const;
    bool isLayout() const;
    bool isUser() const;

    QVariant displayData() const;
    QVariant resourceStatusData() const;
    QVariant resourceExtraStatusData() const;
    QVariant resourceExtraInfoData() const;
    QVariant parentResourceData() const;
    QVariant cameraGroupIdData() const;
    QVariant customGroupIdData() const;
    QVariant resourceIconKeyData() const;

    void initCameraGroupIdNotifications();
    void initCustomGroupIdNotifications();

    virtual void initResourceIconKeyNotifications() const;

    void discardCache(QVariant& cachedValue, const QVector<int>& relevantRoles) const;

protected:
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

    mutable std::once_flag m_resourceExtraInfoFlag;
    mutable QVariant m_resourceExtraInfoCache;

    mutable std::once_flag m_parentResourceFlag;
    mutable QVariant m_parentResourceCache;

    mutable QVariant m_cameraGroupIdCache;
    mutable QVariant m_customGroupIdCache;

    mutable std::once_flag m_resourceIconKeyFlag;
    mutable QVariant m_resourceIconKeyCache;

    mutable nx::utils::ScopedConnections m_connectionsGuard;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::core
