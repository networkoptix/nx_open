// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/resource_views/entity_item_model/item/abstract_item.h>
#include <nx/vms/client/mobile/system_context_aware.h>

namespace nx::vms::client::core::entity_resource_tree { class RecorderItemDataHelper; }

namespace nx::vms::client::mobile {
namespace entity_resource_tree {

class ResourceTreeItemFactory:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;
    using AbstractItemPtr = core::entity_item_model::AbstractItemPtr;

public:
    ResourceTreeItemFactory(SystemContext* systemContext);

    // Resource Tree top level group items.
    AbstractItemPtr createLayoutsItem() const;
    AbstractItemPtr createCamerasItem() const;
    AbstractItemPtr createAllDevicesItem() const;

    // Resource Tree resource based items.
    AbstractItemPtr createResourceItem(const QnResourcePtr& resource);
    AbstractItemPtr createCloudLayoutItem(const QnLayoutResourcePtr& layout);

    // Resource Tree recorder / multi-sensor camera group item.
    AbstractItemPtr createRecorderItem(
        const QString& cameraGroupId,
        const QSharedPointer<core::entity_resource_tree::RecorderItemDataHelper>& recorderItemDataHelper);

    // Resource Tree head item for user defined group.
    AbstractItemPtr createResourceGroupItem(
        const QString& compositeGroupId);
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::mobile
