// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_item_factory.h"

#include <core/resource/layout_resource.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/resource_views/entity_item_model/item/generic_item/generic_item_builder.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/item/core_resource_item.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/item/generic_item_data.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/core/skin/resource_icon_cache.h>
#include <nx/vms/client/mobile/system_context.h>

namespace nx::vms::client::mobile {
namespace entity_resource_tree {

using namespace nx::vms::client::core::entity_item_model;
using namespace nx::vms::client::core::entity_resource_tree;
using NodeType = nx::vms::client::core::ResourceTree::NodeType;
using nx::vms::client::core::ResourceIconCache;

ResourceTreeItemFactory::ResourceTreeItemFactory(SystemContext* systemContext):
    base_type(),
    SystemContextAware(systemContext)
{
}

AbstractItemPtr ResourceTreeItemFactory::createCamerasItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Cameras"))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::camerasAndDevices))
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::Cameras))
        .withFlags({Qt::ItemIsEnabled | Qt::ItemIsSelectable});
}

AbstractItemPtr ResourceTreeItemFactory::createAllDevicesItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("All Devices"))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::camerasAndDevices))
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::Cameras))
        .withFlags({Qt::ItemIsEnabled | Qt::ItemIsSelectable});
}

AbstractItemPtr ResourceTreeItemFactory::createLayoutsItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Layouts"))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::layouts))
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::Layouts))
        .withFlags({Qt::ItemIsEnabled | Qt::ItemIsSelectable});
}

AbstractItemPtr ResourceTreeItemFactory::createResourceItem(const QnResourcePtr& resource)
{
    return std::make_unique<CoreResourceItem>(resource);
}

AbstractItemPtr ResourceTreeItemFactory::createCloudLayoutItem(
    const QnLayoutResourcePtr& layout)
{
    const auto nameProvider = resourceNameProvider(layout);
    const auto nameInvalidator = resourceNameInvalidator(layout);
    const auto iconProvider = cloudLayoutIconProvider(layout);
    const auto iconInvalidator = cloudLayoutIconInvalidator(layout);
    const auto extraStatusProvider = cloudLayoutExtraStatusProvider(layout);
    const auto extraStatusInvalidator = cloudLayoutExtraStatusInvalidator(layout);
    const auto groupIdProvider = cloudLayoutGroupIdProvider(layout);
    const auto groupIdInvalidator = cloudLayoutGroupIdInvalidator(layout);

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, nameProvider, nameInvalidator)
        .withRole(core::ResourceRole, QVariant::fromValue(layout.staticCast<QnResource>()))
        .withRole(core::ResourceIconKeyRole, iconProvider, iconInvalidator)
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::resource))
        .withRole(core::ResourceExtraStatusRole, extraStatusProvider, extraStatusInvalidator)
        .withRole(core::ResourceTreeCustomGroupIdRole, groupIdProvider, groupIdInvalidator)
        .withFlags({Qt::ItemIsEnabled | Qt::ItemIsSelectable});
}

AbstractItemPtr ResourceTreeItemFactory::createRecorderItem(
    const QString& cameraGroupId,
    const QSharedPointer<RecorderItemDataHelper>& recorderItemDataHelper)
{
    const auto nameProvider = recorderNameProvider(cameraGroupId, recorderItemDataHelper);
    const auto nameInvalidator = recorderNameInvalidator(cameraGroupId, recorderItemDataHelper);
    const auto iconProvider = recorderIconProvider(cameraGroupId, recorderItemDataHelper);
    const auto iconInvalidator = recorderIconInvalidator(cameraGroupId, recorderItemDataHelper);

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, nameProvider, nameInvalidator)
        .withRole(core::ResourceIconKeyRole, iconProvider, iconInvalidator)
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::recorder))
        .withRole(core::CameraGroupIdRole, cameraGroupId)
        .withFlags({Qt::ItemIsEnabled | Qt::ItemIsSelectable});
}

AbstractItemPtr ResourceTreeItemFactory::createResourceGroupItem(
    const QString& compositeGroupId)
{
    using namespace nx::vms::client::core::entity_resource_tree::resource_grouping;

    const auto displayText = getResourceTreeDisplayText(compositeGroupId);

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, displayText)
        .withRole(core::ResourceTreeCustomGroupIdRole, compositeGroupId)
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::customResourceGroup))
        .withRole(core::ResourceIconKeyRole, static_cast<int>(core::ResourceIconCache::LocalResources))
        .withFlags({Qt::ItemIsEnabled | Qt::ItemIsSelectable});
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::mobile
