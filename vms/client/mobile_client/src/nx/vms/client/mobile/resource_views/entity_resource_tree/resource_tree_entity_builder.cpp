// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_entity_builder.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/cross_system/resource_tree/cloud_layouts_source.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/core/resource_views/entity_item_model/entity/flattening_group_entity.h>
#include <nx/vms/client/core/resource_views/entity_item_model/entity/grouping_entity.h>
#include <nx/vms/client/core/resource_views/entity_item_model/entity/single_item_entity.h>
#include <nx/vms/client/core/resource_views/entity_item_model/entity/unique_key_list_entity.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/camera_resource_index.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/item_order/resource_tree_item_order.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/recorder_item_data_helper.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_grouping/resource_grouping_data.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_source/accessible_resource_proxy_source.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_source/composite_resource_source.h>
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
    if (!user)
        return {};

    auto layouts = std::make_unique<AccessibleResourceProxySource>(
        systemContext,
        user,
        std::make_unique<LayoutResourceSource>(systemContext->resourcePool(), user, true));

    if (!user->isCloud())
        return std::make_shared<ResourceSourceAdapter>(std::move(layouts));

    auto crossSystemLayouts = std::make_unique<CloudLayoutsSource>();
    auto result = std::make_unique<CompositeResourceSource>(
        std::move(layouts),
        std::move(crossSystemLayouts));

    return std::make_shared<ResourceSourceAdapter>(std::move(result));
}

UniqueResourceSourcePtr camerasSource(
    const QnUserResourcePtr& user,
    SystemContext* systemContext,
    CameraResourceIndex* camerasIndex)
{
    if (!user)
        return {};

    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<AccessibleResourceProxySource>(
            systemContext,
            user,
            std::make_unique<CameraResourceSource>(camerasIndex, QnMediaServerResourcePtr())));
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

    ResourceTreeEntityBuilder::ResourceTreeEntityBuilder(SystemContext* systemContext):
    base_type(),
    SystemContextAware(systemContext),
    m_cameraResourceIndex(new CameraResourceIndex(resourcePool())),
    m_recorderItemDataHelper(new core::entity_resource_tree::RecorderItemDataHelper(m_cameraResourceIndex.get())),
    m_itemFactory(new ResourceTreeItemFactory(systemContext))
{
    auto userWatcher = new core::SessionResourcesSignalListener<QnUserResource>(
        systemContext,
        this);
    userWatcher->setOnRemovedHandler(
        [this](const QnUserResourceList& users)
        {
            if (m_user && users.contains(m_user))
                m_user.reset();
        });
    userWatcher->start();
}

ResourceTreeEntityBuilder::~ResourceTreeEntityBuilder()
{
}

QnUserResourcePtr ResourceTreeEntityBuilder::user() const
{
    return m_user;
}

void ResourceTreeEntityBuilder::setUser(const QnUserResourcePtr& user)
{
    m_user = user;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createTreeEntity() const
{
    auto treeEntity = std::make_unique<CompositionEntity>();
    treeEntity->addSubEntity(createLayoutsGroupEntity());
    treeEntity->addSubEntity(createCamerasGroupEntity());
    return treeEntity;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createCamerasGroupEntity() const
{
    using GroupingRule = GroupingRule<QString, QnResourcePtr>;
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    const auto recorderGroupCreator = recorderGroupItemCreator(
        m_itemFactory.get(),
        m_recorderItemDataHelper);

    GroupingRuleStack groupingRuleStack;

    groupingRuleStack.push_back(
        {resource_grouping::getUserDefinedGroupId,
        userDefinedGroupItemCreator(m_itemFactory.get()),
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
        resourceItemCreator(m_itemFactory.get()),
        core::ResourceRole,
        serverResourcesOrder(),
        groupingRuleStack);

    if (auto source = camerasSource(m_user, systemContext(), m_cameraResourceIndex.get()))
        camerasGroupingEntity->installItemSource(source);

    return makeFlatteningGroup(
        m_itemFactory->createCamerasItem(),
        std::move(camerasGroupingEntity),
        FlatteningGroupEntity::AutoFlatteningPolicy::noPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createLayoutsGroupEntity() const
{
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    auto layoutItemCreator =
        [this, baseItemCreator = resourceItemCreator(m_itemFactory.get())]
        (const QnResourcePtr& resource)
    {
        auto layout = resource.objectCast<QnLayoutResource>();
        NX_ASSERT(layout);
        if (layout->hasFlags(Qn::cross_system))
            return m_itemFactory->createCloudLayoutItem(layout);
        else
            return baseItemCreator(layout);
    };

    GroupingRuleStack groupingRuleStack;
    groupingRuleStack.push_back({resource_grouping::getUserDefinedGroupId,
        userDefinedGroupItemCreator(m_itemFactory.get()),
        core::ResourceTreeCustomGroupIdRole,
        resource_grouping::kUserDefinedGroupingDepth,
        numericOrder()});

    auto layoutsGroupingList = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
        layoutItemCreator,
        core::ResourceRole,
        layoutsOrder(),
        groupingRuleStack);

    if (auto source = layoutsSource(m_user, systemContext()))
        layoutsGroupingList->installItemSource(source);

    auto composition = std::make_unique<CompositionEntity>();
    composition->addSubEntity(makeSingleItemEntity(m_itemFactory->createAllDevicesItem()));
    composition->addSubEntity(std::move(layoutsGroupingList));

    return makeFlatteningGroup(
        m_itemFactory->createLayoutsItem(),
        std::move(composition),
        FlatteningGroupEntity::AutoFlatteningPolicy::noPolicy);
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::mobile
