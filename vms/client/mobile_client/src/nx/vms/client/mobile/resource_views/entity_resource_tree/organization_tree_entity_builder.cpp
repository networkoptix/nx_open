// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "organization_tree_entity_builder.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/cross_system/resource_tree/cloud_layouts_source.h>
#include <nx/vms/client/core/cross_system/resource_tree/cloud_systems_source.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/core/resource_views/entity_item_model/entity/flattening_group_entity.h>
#include <nx/vms/client/core/resource_views/entity_item_model/entity/grouping_entity.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/camera_resource_index.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/item_order/resource_tree_item_order.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/recorder_item_data_helper.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_grouping/resource_grouping_data.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_source/resource_sources/camera_resource_source.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_source/resource_sources/layout_resource_source.h>
#include <nx/vms/client/mobile/resource_views/entity_resource_tree/resource_tree_item_factory.h>
#include <nx/vms/client/mobile/system_context.h>

namespace {

using namespace nx::vms::client::core;
using namespace nx::vms::client::core::entity_item_model;
using namespace nx::vms::client::core::entity_resource_tree;
using namespace nx::vms::client::mobile::entity_resource_tree;

using NodeType = ResourceTree::NodeType;
using ResourceItemCreator = std::function<AbstractItemPtr(const QnResourcePtr&)>;

using GroupItemCreator = std::function<AbstractItemPtr(const QString&)>;

UniqueResourceSourcePtr layoutsSource(const QnUserResourcePtr& user, SystemContext* systemContext)
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<LayoutResourceSource>(systemContext->resourcePool(), user, true));
}

UniqueResourceSourcePtr camerasSource(
    SystemContext* /*systemContext*/,
    CameraResourceIndex* camerasIndex)
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<CameraResourceSource>(camerasIndex, QnMediaServerResourcePtr()));
}

ResourceItemCreator resourceItemCreator(ResourceTreeItemFactory* factory)
{
    return
        [factory](const QnResourcePtr& resource) { return factory->createResourceItem(resource); };
}

GroupItemCreator recorderGroupItemCreator(
    ResourceTreeItemFactory* factory,
    const QSharedPointer<RecorderItemDataHelper>& recorderResourceIndex)
{
    return
        [factory, recorderResourceIndex](const QString& groupId) -> AbstractItemPtr
        {
            return factory->createRecorderItem(groupId, recorderResourceIndex);
        };
}

GroupItemCreator userDefinedGroupItemCreator(ResourceTreeItemFactory* factory)
{
    return
        [factory](const QString& compositeGroupId) -> AbstractItemPtr
        {
            return factory->createResourceGroupItem(compositeGroupId);
        };
}

} // namespace

namespace nx::vms::client::mobile {
namespace entity_resource_tree {

struct SystemHelpers
{
    std::unique_ptr<core::entity_resource_tree::CameraResourceIndex> cameraResourceIndex;
    QSharedPointer<core::entity_resource_tree::RecorderItemDataHelper> recorderItemDataHelper;
};

struct OrganizationTreeEntityBuilder::Private
{
    nx::Uuid m_organizationId;
    std::unique_ptr<ResourceTreeItemFactory> itemFactory;
    mutable std::map<core::SystemContext*, SystemHelpers> helpers; //< TODO: #amalov Attach to source.
};

OrganizationTreeEntityBuilder::OrganizationTreeEntityBuilder():
    base_type(),
    d(new Private{
        .itemFactory = std::make_unique<ResourceTreeItemFactory>(
            appContext()->currentSystemContext())
    })
{
    // TODO: #amalov Share item factory between builders. Make it system context unaware.
}

OrganizationTreeEntityBuilder::~OrganizationTreeEntityBuilder()
{
}

AbstractEntityPtr OrganizationTreeEntityBuilder::createRootTreeEntity(nx::Uuid organizationId) const
{
    auto systemItemCreator =
        [this](const QString& systemId) -> AbstractItemPtr
        {
            return d->itemFactory->createCloudSystemItem(systemId);
        };

    auto systemContentCreator =
        [this](const QString& systemId) -> AbstractEntityPtr
        {
            return createResourceTreeEntity(appContext()->systemContextByCloudSystemId(systemId));
        };

    auto cloudSystemsEntity =
        makeUniqueKeyGroupList<QString>(systemItemCreator, systemContentCreator, numericOrder());

    cloudSystemsEntity->installItemSource(
        std::make_shared<core::entity_resource_tree::CloudSystemsSource>(organizationId));

    return cloudSystemsEntity;
}

AbstractEntityPtr OrganizationTreeEntityBuilder::createResourceTreeEntity(
    core::SystemContext* systemContext) const
{
    auto& helper = d->helpers[systemContext];
    if (!helper.cameraResourceIndex)
    {
        helper.cameraResourceIndex =
            std::make_unique<core::entity_resource_tree::CameraResourceIndex>(
                systemContext->resourcePool());
    }

    auto treeEntity = std::make_unique<CompositionEntity>();
    treeEntity->addSubEntity(createLayoutsGroupEntity(systemContext));
    treeEntity->addSubEntity(createCamerasGroupEntity(systemContext));

    return treeEntity;
}

AbstractEntityPtr OrganizationTreeEntityBuilder::createCamerasGroupEntity(
    core::SystemContext* systemContext) const
{
    using GroupingRule = GroupingRule<QString, QnResourcePtr>;
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    const auto recorderGroupCreator = recorderGroupItemCreator(
        d->itemFactory.get(),
        d->helpers.at(systemContext).recorderItemDataHelper);

    GroupingRuleStack groupingRuleStack;

    groupingRuleStack.push_back(
        {resource_grouping::getUserDefinedGroupId,
        userDefinedGroupItemCreator(d->itemFactory.get()),
        core::ResourceTreeCustomGroupIdRole,
        resource_grouping::kUserDefinedGroupingDepth,
        numericOrder()});

    const GroupingRule recordersGroupingRule =
        {resource_grouping::getRecorderCameraGroupId,
        recorderGroupCreator,
        core::CameraGroupIdRole,
        1, //< Dimension.
        numericOrder()};
    groupingRuleStack.push_back(recordersGroupingRule);

    auto camerasGroupingEntity = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
        resourceItemCreator(d->itemFactory.get()),
        core::ResourceRole,
        serverResourcesOrder(),
        groupingRuleStack);

    if (auto source = camerasSource(systemContext, d->helpers.at(systemContext).cameraResourceIndex.get()))
        camerasGroupingEntity->installItemSource(source);

    return makeFlatteningGroup(
        d->itemFactory->createCamerasItem(),
        std::move(camerasGroupingEntity),
        FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy);
}

AbstractEntityPtr OrganizationTreeEntityBuilder::createLayoutsGroupEntity(
    core::SystemContext* systemContext) const
{
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    const auto layoutItemCreator =
        [this, baseItemCreator = resourceItemCreator(d->itemFactory.get())]
        (const QnResourcePtr& resource)
    {
        const auto layout = resource.objectCast<QnLayoutResource>();
        NX_ASSERT(layout);
        if (layout->hasFlags(Qn::cross_system))
            return d->itemFactory->createCloudLayoutItem(layout);
        else
            return baseItemCreator(layout);
    };

    GroupingRuleStack groupingRuleStack;
    groupingRuleStack.push_back({resource_grouping::getUserDefinedGroupId,
        userDefinedGroupItemCreator(d->itemFactory.get()),
        core::ResourceTreeCustomGroupIdRole,
        resource_grouping::kUserDefinedGroupingDepth,
        numericOrder()});

    auto layoutsGroupingList = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
        layoutItemCreator,
        core::ResourceRole,
        layoutsOrder(),
        groupingRuleStack);

    if (const auto source = layoutsSource(/*user*/ {}, systemContext))
        layoutsGroupingList->installItemSource(source);

    auto composition = std::make_unique<CompositionEntity>();
    composition->addSubEntity(std::move(layoutsGroupingList));

    return makeFlatteningGroup(
        d->itemFactory->createLayoutsItem(),
        std::move(composition),
        FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy);
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::mobile
