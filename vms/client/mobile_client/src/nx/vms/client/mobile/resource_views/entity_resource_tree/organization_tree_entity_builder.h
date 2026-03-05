// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/core/resource_views/entity_item_model/entity/abstract_entity.h>
#include <nx/vms/client/core/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::client::core { class SystemContext; }

namespace nx::vms::client::core::entity_resource_tree {

class CameraResourceIndex;
class RecorderItemDataHelper;
class ResourceTreeItemKeySourcePool;

} // namespace nx::vms::client::core::entity_resource_tree

namespace nx::vms::client::mobile {
namespace entity_resource_tree {

class ResourceTreeItemFactory;

class OrganizationTreeEntityBuilder: public QObject
{
    Q_OBJECT
    using base_type = QObject;
    using AbstractEntityPtr = core::entity_item_model::AbstractEntityPtr;
    using AbstractItemPtr = core::entity_item_model::AbstractItemPtr;

public:
    OrganizationTreeEntityBuilder();
    virtual ~OrganizationTreeEntityBuilder() override;

    AbstractEntityPtr createRootTreeEntity(nx::Uuid organizationId) const;
public:
    AbstractEntityPtr createResourceTreeEntity(core::SystemContext* systemContext) const;
    AbstractEntityPtr createCamerasGroupEntity(core::SystemContext* systemContext) const;
    AbstractEntityPtr createLayoutsGroupEntity(core::SystemContext* systemContext) const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::mobile
