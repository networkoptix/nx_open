// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/shared_item/shared_item.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class RecorderItemDataHelper;

class ResourceTreeItemFactory: public QObject, public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;
    using AbstractItemPtr = entity_item_model::AbstractItemPtr;

public:
    ResourceTreeItemFactory(SystemContext* systemContext);

    // Resource Tree header items.
    AbstractItemPtr createCurrentSystemItem() const;
    AbstractItemPtr createCurrentUserItem(const QnUserResourcePtr& user) const;
    AbstractItemPtr createSpacerItem() const;
    AbstractItemPtr createSeparatorItem() const;

    // Resource Tree top level group items.
    AbstractItemPtr createServersItem() const;
    AbstractItemPtr createCamerasAndDevicesItem(Qt::ItemFlags itemFlags) const;
    AbstractItemPtr createLayoutsItem() const;
    AbstractItemPtr createShowreelsItem() const;
    AbstractItemPtr createIntegrationsItem(Qt::ItemFlags flags) const;
    AbstractItemPtr createWebPagesItem(Qt::ItemFlags flags) const;
    AbstractItemPtr createWebPagesAndIntegrationsItem(Qt::ItemFlags flags) const;
    AbstractItemPtr createUsersItem() const;
    AbstractItemPtr createOtherSystemsItem() const;
    AbstractItemPtr createLocalFilesItem() const;
    AbstractItemPtr createHealthMonitorsItem() const;
    AbstractItemPtr createVideoWallsItem(QString customName = QString()) const;

    // Resource Tree childless disabled user resources placeholders.
    AbstractItemPtr createAllCamerasAndResourcesItem() const;
    AbstractItemPtr createAllSharedLayoutsItem() const;

    // Resource Tree user resources group items.
    AbstractItemPtr createSharedResourcesItem() const;
    AbstractItemPtr createSharedLayoutsItem(bool useRegularAppearance) const;

    // Resource Tree resource based items.
    AbstractItemPtr createResourceItem(const QnResourcePtr& resource);

    // Resource Tree head item for user defined group.
    AbstractItemPtr createResourceGroupItem(
        const QString& compositeGroupId,
        Qt::ItemFlags itemFlags);

    // Resource Tree recorder / multi-sensor camera group item.
    AbstractItemPtr createRecorderItem(
        const QString& cameraGroupId,
        const QSharedPointer<RecorderItemDataHelper>& recorderItemDataHelper,
        Qt::ItemFlags itemFlags);

    // Resource Tree other system group item.
    AbstractItemPtr createOtherSystemItem(const QString& systemId);

    // Resource Tree cloud system item within 'Other Systems' group.
    AbstractItemPtr createCloudSystemItem(const QString& systemId);

    // Resource Tree showreel item.
    AbstractItemPtr createShowreelItem(const QnUuid& showreelUuid);

    // Resource Tree video wall screen item.
    AbstractItemPtr createVideoWallScreenItem(const QnVideoWallResourcePtr& videoWall,
        const QnUuid& screenUuid);

    // Resource Tree video wall matrix item.
    AbstractItemPtr createVideoWallMatrixItem(const QnVideoWallResourcePtr& videoWall,
        const QnUuid& matrixUuid);

    // Resource Tree Cross-System Resources status preloader for the Cloud System.
    AbstractItemPtr createCloudSystemStatusItem(const QString& systemId);

    // Resource Tree Cloud Layout item.
    AbstractItemPtr createCloudLayoutItem(const QnLayoutResourcePtr& layout);

    AbstractItemPtr createOtherServerItem(const QnUuid& serverId);

private:
    QHash<QnResourcePtr, entity_item_model::SharedItemOriginPtr> m_resourceItemCache;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
