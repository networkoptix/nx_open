// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/core/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/core/resource_views/entity_item_model/entity/abstract_entity.h>
#include <nx/vms/client/core/resource_views/entity_item_model/item/abstract_item.h>
#include <nx/vms/client/mobile/system_context_aware.h>

namespace nx::vms::client::core::entity_resource_tree {

class CameraResourceIndex;
class RecorderItemDataHelper;
class ResourceTreeItemKeySourcePool;

} // namespace nx::vms::client::core::entity_resource_tree

namespace nx::vms::client::mobile {
namespace entity_resource_tree {

class ResourceTreeItemFactory;

class ResourceTreeEntityBuilder:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;
    using AbstractEntityPtr = core::entity_item_model::AbstractEntityPtr;
    using AbstractItemPtr = core::entity_item_model::AbstractItemPtr;

public:
    ResourceTreeEntityBuilder(SystemContext* systemContext);
    ~ResourceTreeEntityBuilder();

    QnUserResourcePtr user() const;
    void setUser(const QnUserResourcePtr& user);

public:
    AbstractEntityPtr createTreeEntity() const;
    AbstractEntityPtr createCamerasGroupEntity() const;
    AbstractEntityPtr createLayoutsGroupEntity() const;

private:
    QScopedPointer<core::entity_resource_tree::CameraResourceIndex> m_cameraResourceIndex;
    QSharedPointer<core::entity_resource_tree::RecorderItemDataHelper> m_recorderItemDataHelper;
    QScopedPointer<ResourceTreeItemFactory> m_itemFactory;
    QnUserResourcePtr m_user;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::mobile
