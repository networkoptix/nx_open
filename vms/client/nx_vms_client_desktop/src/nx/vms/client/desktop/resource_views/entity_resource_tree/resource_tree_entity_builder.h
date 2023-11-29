// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/abstract_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class CameraResourceIndex;
class UserLayoutResourceIndex;
class RecorderItemDataHelper;
class ResourceTreeItemFactory;
class ResourceTreeItemKeySourcePool;

class NX_VMS_CLIENT_DESKTOP_API ResourceTreeEntityBuilder:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;
    using AbstractEntityPtr = entity_item_model::AbstractEntityPtr;
    using AbstractItemPtr = entity_item_model::AbstractItemPtr;


public:
    ResourceTreeEntityBuilder(SystemContext* systemContext);
    ~ResourceTreeEntityBuilder();

    QnUserResourcePtr user() const;
    void setUser(const QnUserResourcePtr& user);

public:
    AbstractEntityPtr createCurrentSystemEntity() const;
    AbstractEntityPtr createCurrentUserEntity() const;
    AbstractEntityPtr createSpacerEntity() const;
    AbstractEntityPtr createSeparatorEntity() const;

    AbstractEntityPtr createServersGroupEntity(bool showProxiedResources) const;
    AbstractEntityPtr createCamerasAndDevicesGroupEntity(bool showProxiedResources) const;
    AbstractEntityPtr createLayoutsGroupEntity() const;

    AbstractEntityPtr createShowreelsGroupEntity() const;
    AbstractEntityPtr createIntegrationsGroupEntity() const;
    AbstractEntityPtr createWebPagesGroupEntity() const;
    AbstractEntityPtr createLocalFilesGroupEntity() const;
    AbstractEntityPtr createLocalOtherSystemsEntity() const;
    AbstractEntityPtr createCloudOtherSystemsEntity() const;
    AbstractEntityPtr createCloudSystemCamerasEntity(const QString& systemId) const;
    AbstractEntityPtr createOtherSystemsGroupEntity() const;

    AbstractEntityPtr createDialogAllCamerasEntity(
        bool showServers,
        const std::function<bool(const QnResourcePtr&)>& resourceFilter) const;

    AbstractEntityPtr createDialogAllCamerasAndResourcesEntity(
        bool withProxiedWebPages = true) const;

    AbstractEntityPtr createDialogServerCamerasEntity(
        const QnMediaServerResourcePtr& server,
        const std::function<bool(const QnResourcePtr&)>& resourceFilter) const;

    AbstractEntityPtr createDialogAllLayoutsEntity() const;
    AbstractEntityPtr createDialogShareableLayoutsEntity(
        const std::function<bool(const QnResourcePtr&)>& resourceFilter) const;

    AbstractEntityPtr createDialogHotspotTargetsEntity(
        bool showServers,
        const std::function<bool(const QnResourcePtr&)>& resourceFilter) const;

    AbstractEntityPtr createAllServersEntity() const;
    AbstractEntityPtr createAllCamerasEntity() const;
    AbstractEntityPtr createAllLayoutsEntity() const;

    AbstractEntityPtr createFlatCamerasListEntity() const;

    AbstractEntityPtr createServerCamerasEntity(
        const QnMediaServerResourcePtr& server,
        bool showProxiedResources) const;

    AbstractEntityPtr createVideowallsEntity() const;
    AbstractEntityPtr createLayoutItemListEntity(const QnResourcePtr& layoutResource) const;

    AbstractEntityPtr createDialogEntities(ResourceTree::ResourceFilters resourceTypes,
        bool alwaysCreateGroupElements = false, bool combineWebPagesAndIntegrations = false) const;

    AbstractEntityPtr addPinnedItem(AbstractEntityPtr baseEntity, AbstractItemPtr pinnedItem) const;

private:
    bool hasPowerUserPermissions() const;

private:
    QScopedPointer<CameraResourceIndex> m_cameraResourceIndex;
    QScopedPointer<UserLayoutResourceIndex> m_userLayoutResourceIndex;
    QSharedPointer<RecorderItemDataHelper> m_recorderItemDataHelper;
    QScopedPointer<ResourceTreeItemFactory> m_itemFactory;
    QScopedPointer<ResourceTreeItemKeySourcePool> m_itemKeySourcePool;
    QnUserResourcePtr m_user;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
