// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_entity_builder.h"

#include <client/client_globals.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/fake_media_server.h>
#include <core/resource/layout_resource.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <finders/systems_finder.h>
#include <network/system_description.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/base_notification_observer.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/composition_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/flattening_group_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/grouping_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/single_item_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_group_list_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_list_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/camera_resource_index.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/entity/layout_item_list_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/entity/showreels_list_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/entity/videowall_matrices_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/entity/videowall_screens_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/cloud_cross_system_camera_decorator.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/health_monitor_resource_item_decorator.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/main_tree_resource_item_decorator.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item_order/resource_tree_item_order.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/user_layout_resource_index.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/recorder_item_data_helper.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_source/resource_tree_item_key_source_pool.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_item_factory.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>

using namespace nx::vms::client::desktop;

namespace {

using namespace entity_item_model;
using namespace entity_resource_tree;
using namespace nx::vms::api;

using NodeType = ResourceTree::NodeType;
using ResourceItemCreator = std::function<AbstractItemPtr(const QnResourcePtr&)>;

using UserdDefinedGroupIdGetter = std::function<QString(const QnResourcePtr&, int)>;
using UserdDefinedGroupItemCreator = std::function<AbstractItemPtr(const QString&)>;

using RecorderCameraGroupIdGetter = std::function<QString(const QnResourcePtr&, int)>;
using RecorderGroupItemCreator = std::function<AbstractItemPtr(const QString&)>;

ResourceItemCreator serverResourceItemCreator(
    ResourceTreeItemFactory* factory,
    GlobalPermissions permissions)
{
    return
        [factory, permissions](const QnResourcePtr& resource)
        {
            const auto isEdgeCamera =
                !resource->hasFlags(Qn::server) && resource.dynamicCast<QnVirtualCameraResource>();

            const auto nodeType = isEdgeCamera ? NodeType::edge : NodeType::resource;

            return std::make_unique<MainTreeResourceItemDecorator>(
                factory->createResourceItem(resource), permissions, nodeType);
        };
}

ResourceItemCreator simpleResourceItemCreator(ResourceTreeItemFactory* factory)
{
    return
        [factory](const QnResourcePtr& resource) { return factory->createResourceItem(resource); };
}

ResourceItemCreator resourceItemCreator(
    ResourceTreeItemFactory* factory,
    GlobalPermissions permissions)
{
    return
        [factory, permissions](const QnResourcePtr& resource)
        {
            return std::make_unique<MainTreeResourceItemDecorator>(
                factory->createResourceItem(resource), permissions, NodeType::resource);
        };
}

ResourceItemCreator sharedResourceItemCreator(
    ResourceTreeItemFactory* factory,
    GlobalPermissions permissions)
{
    return
        [factory, permissions](const QnResourcePtr& resource)
        {
            return std::make_unique<MainTreeResourceItemDecorator>(
                factory->createResourceItem(resource), permissions, NodeType::sharedResource);
        };
}

ResourceItemCreator subjectLayoutItemCreator(
    ResourceTreeItemFactory* factory,
    GlobalPermissions permissions)
{
    return
        [factory, permissions](const QnResourcePtr& resource)
        {
            const auto nodeType = resource.staticCast<QnLayoutResource>()->isShared()
                ? NodeType::sharedLayout
                : NodeType::resource;

            return std::make_unique<MainTreeResourceItemDecorator>(
                factory->createResourceItem(resource), permissions, nodeType);
        };
}

LayoutItemCreator layoutItemCreator(
    ResourceTreeItemFactory* factory,
    GlobalPermissions permissions,
    const QnLayoutResourcePtr& layout)
{
    return
        [factory, permissions, layout](const QnUuid& itemId) -> AbstractItemPtr
        {
            const auto itemData = layout->getItem(itemId);

            const auto itemResource = getResourceByDescriptor(itemData.resource);
            if (!itemResource)
                return {};

            return std::make_unique<MainTreeResourceItemDecorator>(
                factory->createResourceItem(itemResource),
                permissions,
                NodeType::layoutItem,
                itemData.uuid);
        };
}

//-------------------------------------------------------------------------------------------------
// Function wrapper providers related to Recorders and Multisensor cameras.
//-------------------------------------------------------------------------------------------------

RecorderCameraGroupIdGetter recorderCameraGroupIdGetter()
{
    return
        [](const QnResourcePtr& resource, int /*order*/)
        {
            if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
                return camera->getGroupId();
            return QString();
        };
}

RecorderGroupItemCreator recorderGroupItemCreator(
    ResourceTreeItemFactory* factory,
    const QSharedPointer<RecorderItemDataHelper>& recorderResourceIndex,
    GlobalPermissions permissions)
{
    return
        [factory, recorderResourceIndex, permissions](const QString& groupId) -> AbstractItemPtr
        {
            Qt::ItemFlags flags = {Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemIsDragEnabled};
            flags.setFlag(Qt::ItemIsEditable, permissions.testFlag(GlobalPermission::editCameras));

            return factory->createRecorderItem(groupId, recorderResourceIndex, flags);
        };
}

//-------------------------------------------------------------------------------------------------
// Function wrapper providers related to the custom grouping within Resource Tree
//-------------------------------------------------------------------------------------------------

UserdDefinedGroupIdGetter userDefinedGroupIdGetter()
{
    return
        [](const QnResourcePtr& resource, int order)
        {
            const auto compositeGroupId = resource_grouping::resourceCustomGroupId(resource);

            if (resource_grouping::compositeIdDimension(compositeGroupId) <= order)
                return QString();

            return resource_grouping::trimCompositeId(compositeGroupId, order + 1);
        };
}

UserdDefinedGroupItemCreator userDefinedGroupItemCreator(
    ResourceTreeItemFactory* factory,
    GlobalPermissions permissions)
{
    return
        [factory, permissions](const QString& compositeGroupId) -> AbstractItemPtr
        {
            Qt::ItemFlags result =
                {Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemIsDragEnabled};

            if (permissions.testFlag(GlobalPermission::admin))
                result |= {Qt::ItemIsEditable, Qt::ItemIsDropEnabled};

            return factory->createResourceGroupItem(compositeGroupId, result);
        };
}

//-------------------------------------------------------------------------------------------------
// Function wrapper providers related to the grouping by server in resource selection dialogs.
//-------------------------------------------------------------------------------------------------
std::function<QString(const QnResourcePtr&, int)> parentServerIdGetter()
{
    return
        [](const QnResourcePtr& resource, int /*order*/)
        {
            return resource->getParentId().toString();
        };
}

std::function<AbstractItemPtr(const QString&)> parentServerItemCreator(
    const QnResourcePool* resourcePool, ResourceTreeItemFactory* factory)
{
    return
        [resourcePool, factory](const QString& serverId) -> AbstractItemPtr
        {
            const auto serverResource =
                resourcePool->getResourceById(QnUuid::fromStringSafe(serverId));
            return factory->createResourceItem(serverResource);
        };
}

} // namespace

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace entity_item_model;
using namespace nx::vms::api;

ResourceTreeEntityBuilder::ResourceTreeEntityBuilder(const QnCommonModule* commonModule):
    base_type(),
    m_commonModule(commonModule),
    m_cameraResourceIndex(new CameraResourceIndex(commonModule->resourcePool())),
    m_userLayoutResourceIndex(new UserLayoutResourceIndex(commonModule->resourcePool())),
    m_recorderItemDataHelper(new RecorderItemDataHelper(m_cameraResourceIndex.get())),
    m_itemFactory(new ResourceTreeItemFactory(commonModule)),
    m_itemKeySourcePool(new ResourceTreeItemKeySourcePool(
        commonModule, m_cameraResourceIndex.get(), m_userLayoutResourceIndex.get()))
{
    // Message processor does not exist in unit tests.
    if (auto messageProcessor = m_commonModule->messageProcessor())
    {
        auto userWatcher = new core::SessionResourcesSignalListener<QnUserResource>(
            m_commonModule->resourcePool(),
            messageProcessor,
            this);
        userWatcher->setOnRemovedHandler(
            [this](const QnUserResourceList& users)
            {
                if (m_user && users.contains(m_user))
                    m_user.reset();
            });
        userWatcher->start();
    }
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

GlobalPermissions ResourceTreeEntityBuilder::userGlobalPermissions() const
{
    return !user().isNull()
        ? m_commonModule->globalPermissionsManager()->globalPermissions(user())
        : GlobalPermission::none;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createServersGroupEntity() const
{
    auto serverExpander =
        [this](const QnResourcePtr& resource)
        {
            if (!resource->hasFlags(Qn::server))
                return AbstractEntityPtr();
            return createServerCamerasEntity(resource.staticCast<QnMediaServerResource>());
        };

    auto serversGroupList = makeUniqueKeyGroupList<QnResourcePtr>(
        serverResourceItemCreator(m_itemFactory.get(), userGlobalPermissions()),
        serverExpander,
        serversOrder());

    const bool showReducedEdgeServers =
        !userGlobalPermissions().testFlag(GlobalPermission::customUser);

    serversGroupList->installItemSource(
        m_itemKeySourcePool->serversSource(user(), showReducedEdgeServers));

    return makeFlatteningGroup(
        m_itemFactory->createServersItem(),
        std::move(serversGroupList),
        FlatteningGroupEntity::AutoFlatteningPolicy::singleChildPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createCamerasAndDevicesGroupEntity() const
{
    Qt::ItemFlags camerasAndDevicesitemFlags = {Qt::ItemIsEnabled, Qt::ItemIsSelectable};
    if (userGlobalPermissions().testFlag(GlobalPermission::admin))
        camerasAndDevicesitemFlags |= {Qt::ItemIsDropEnabled};

    return makeFlatteningGroup(
        m_itemFactory->createCamerasAndDevicesItem(camerasAndDevicesitemFlags),
        createServerCamerasEntity(QnMediaServerResourcePtr()),
        FlatteningGroupEntity::AutoFlatteningPolicy::noPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createDialogAllCamerasEntity(
    bool showServers,
    const std::function<bool(const QnResourcePtr&)>& resourceFilter) const
{
    using GroupingRule = GroupingRule<QString, QnResourcePtr>;
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    const auto recorderGroupCreator = recorderGroupItemCreator(
        m_itemFactory.get(),
        m_recorderItemDataHelper,
        GlobalPermissions());

    GroupingRuleStack groupingRuleStack;

    if (showServers)
    {
        groupingRuleStack.push_back(
            {parentServerIdGetter(),
            parentServerItemCreator(m_commonModule->resourcePool(), m_itemFactory.get()),
            Qn::ParentResourceRole,
            1,
            numericOrder()});
    }

    groupingRuleStack.push_back(
        {userDefinedGroupIdGetter(),
        userDefinedGroupItemCreator(m_itemFactory.get(), GlobalPermissions()),
        Qn::ResourceTreeCustomGroupIdRole,
        resource_grouping::kUserDefinedGroupingDepth,
        numericOrder()});

    const GroupingRule recordersGroupingRule =
        {recorderCameraGroupIdGetter(),
        recorderGroupCreator,
        Qn::CameraGroupIdRole,
        1, //< Dimension.
        numericOrder()};
    groupingRuleStack.push_back(recordersGroupingRule);

    auto camerasGroupingEntity = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
        simpleResourceItemCreator(m_itemFactory.get()),
        Qn::ResourceRole,
        serverResourcesOrder(),
        groupingRuleStack);

    camerasGroupingEntity->installItemSource(
        m_itemKeySourcePool->allDevicesSource(user(), resourceFilter));

    return camerasGroupingEntity;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createDialogAllCamerasAndResourcesEntity() const
{
    using GroupingRule = GroupingRule<QString, QnResourcePtr>;
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    const auto recorderGroupCreator = recorderGroupItemCreator(
        m_itemFactory.get(),
        m_recorderItemDataHelper,
        GlobalPermissions());

    GroupingRuleStack groupingRuleStack;

    groupingRuleStack.push_back(
        {userDefinedGroupIdGetter(),
        userDefinedGroupItemCreator(m_itemFactory.get(), GlobalPermissions()),
        Qn::ResourceTreeCustomGroupIdRole,
        resource_grouping::kUserDefinedGroupingDepth,
        numericOrder()});

    const GroupingRule recordersGroupingRule =
        {recorderCameraGroupIdGetter(),
        recorderGroupCreator,
        Qn::CameraGroupIdRole,
        1, //< Dimension.
        numericOrder()};
    groupingRuleStack.push_back(recordersGroupingRule);

    auto camerasGroupingEntity = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
        simpleResourceItemCreator(m_itemFactory.get()),
        Qn::ResourceRole,
        serverResourcesOrder(),
        groupingRuleStack);

    camerasGroupingEntity->installItemSource(
        m_itemKeySourcePool->devicesAndProxiedWebPagesSource(
        QnMediaServerResourcePtr(),
        user()));

    return camerasGroupingEntity;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createDialogServerCamerasEntity(
    const QnMediaServerResourcePtr& server,
    const std::function<bool(const QnResourcePtr&)>& resourceFilter) const
{
    using GroupingRule = GroupingRule<QString, QnResourcePtr>;
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    const auto recorderGroupCreator = recorderGroupItemCreator(
        m_itemFactory.get(),
        m_recorderItemDataHelper,
        GlobalPermissions());

    GroupingRuleStack groupingRuleStack;

    groupingRuleStack.push_back(
        {userDefinedGroupIdGetter(),
        userDefinedGroupItemCreator(m_itemFactory.get(), GlobalPermissions()),
        Qn::ResourceTreeCustomGroupIdRole,
        resource_grouping::kUserDefinedGroupingDepth,
        numericOrder()});

    const GroupingRule recordersGroupingRule =
        {recorderCameraGroupIdGetter(),
        recorderGroupCreator,
        Qn::CameraGroupIdRole,
        1, //< Dimension.
        numericOrder()};
    groupingRuleStack.push_back(recordersGroupingRule);

    auto camerasGroupingEntity = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
        simpleResourceItemCreator(m_itemFactory.get()),
        Qn::ResourceRole,
        serverResourcesOrder(),
        groupingRuleStack);

    camerasGroupingEntity->installItemSource(
        m_itemKeySourcePool->devicesSource(user(), server, resourceFilter));

    return camerasGroupingEntity;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createDialogShareableMediaEntity() const
{
    auto shareableMediaComposition = std::make_unique<CompositionEntity>();
    shareableMediaComposition->addSubEntity(createDialogAllCamerasAndResourcesEntity());

    const auto healthMonitorItemCreator =
        [this](const QnResourcePtr& resource)
        {
            return std::make_unique<HealthMonitorResourceItemDecorator>(
                m_itemFactory->createResourceItem(resource));
        };
    auto healthMonitorsList = makeKeyList<QnResourcePtr>(
        healthMonitorItemCreator,
        serversOrder());
    healthMonitorsList->installItemSource(
        m_itemKeySourcePool->serversSource(user(), /*showReducedEdgeServers*/ false));
    shareableMediaComposition->addSubEntity(std::move(healthMonitorsList));

    auto webPagesList = makeKeyList<QnResourcePtr>(
        simpleResourceItemCreator(m_itemFactory.get()),
        numericOrder());
    webPagesList->installItemSource(
        m_itemKeySourcePool->webPagesSource(user(), /*includeProxiedWebPages*/ false));
    shareableMediaComposition->addSubEntity(std::move(webPagesList));

    return shareableMediaComposition;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createDialogAllLayoutsEntity() const
{
    auto allLayoutsList = makeKeyList<QnResourcePtr>(
        simpleResourceItemCreator(m_itemFactory.get()), layoutsOrder());

    allLayoutsList->installItemSource(m_itemKeySourcePool->allLayoutsSource(user()));

    return allLayoutsList;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createDialogShareableLayoutsEntity() const
{
    auto allLayoutsList = makeKeyList<QnResourcePtr>(
        simpleResourceItemCreator(m_itemFactory.get()), layoutsOrder());

    allLayoutsList->installItemSource(m_itemKeySourcePool->shareableLayoutsSource(user()));

    return allLayoutsList;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createAllServersEntity() const
{
    auto allServersList = makeKeyList<QnResourcePtr>(
        resourceItemCreator(m_itemFactory.get(), userGlobalPermissions()), numericOrder());

    allServersList->installItemSource(
        m_itemKeySourcePool->serversSource(user(), /*withEdgeServers*/ true));

    return allServersList;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createAllCamerasEntity() const
{
    using GroupingRule = GroupingRule<QString, QnResourcePtr>;
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    const auto recorderGroupCreator = recorderGroupItemCreator(
        m_itemFactory.get(),
        m_recorderItemDataHelper,
        userGlobalPermissions());

    const GroupingRule recordersGroupingRule =
        {recorderCameraGroupIdGetter(),
        recorderGroupCreator,
        Qn::CameraGroupIdRole,
        1, //< Dimension.
        numericOrder()};

    auto camerasGroupingEntity = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
        resourceItemCreator(m_itemFactory.get(), userGlobalPermissions()),
        Qn::ResourceRole,
        serverResourcesOrder(),
        GroupingRuleStack{recordersGroupingRule});

    camerasGroupingEntity->installItemSource(
        m_itemKeySourcePool->allDevicesSource(user(), /*resourceFilter*/ {}));

    return camerasGroupingEntity;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createAllLayoutsEntity() const
{
    auto allLayoutsList = makeKeyList<QnResourcePtr>(
        resourceItemCreator(m_itemFactory.get(), userGlobalPermissions()),
        layoutsOrder());

    allLayoutsList->installItemSource(m_itemKeySourcePool->allLayoutsSource(user()));

    return allLayoutsList;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createServerCamerasEntity(
    const QnMediaServerResourcePtr& server) const
{
    using GroupingRule = GroupingRule<QString, QnResourcePtr>;
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    const auto recorderGroupCreator = recorderGroupItemCreator(
        m_itemFactory.get(),
        m_recorderItemDataHelper,
        userGlobalPermissions());

    GroupingRuleStack groupingRuleStack;

    groupingRuleStack.push_back(
        {userDefinedGroupIdGetter(),
        userDefinedGroupItemCreator(m_itemFactory.get(), userGlobalPermissions()),
        Qn::ResourceTreeCustomGroupIdRole,
        resource_grouping::kUserDefinedGroupingDepth,
        numericOrder()});

    const GroupingRule recordersGroupingRule =
        {recorderCameraGroupIdGetter(),
        recorderGroupCreator,
        Qn::CameraGroupIdRole,
        1, //< Dimension.
        numericOrder()};
    groupingRuleStack.push_back(recordersGroupingRule);

    auto camerasGroupingEntity = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
        resourceItemCreator(m_itemFactory.get(), userGlobalPermissions()),
        Qn::ResourceRole,
        serverResourcesOrder(),
        groupingRuleStack);

    camerasGroupingEntity->installItemSource(
        m_itemKeySourcePool->devicesAndProxiedWebPagesSource(server, user()));

    return camerasGroupingEntity;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createCurrentSystemEntity() const
{
    return makeSingleItemEntity(m_itemFactory->createCurrentSystemItem());
}

AbstractEntityPtr ResourceTreeEntityBuilder::createCurrentUserEntity() const
{
    return makeSingleItemEntity(m_itemFactory->createCurrentUserItem(user()));
}

AbstractEntityPtr ResourceTreeEntityBuilder::createSpacerEntity() const
{
    return makeSingleItemEntity(m_itemFactory->createSpacerItem());
}

AbstractEntityPtr ResourceTreeEntityBuilder::createSeparatorEntity() const
{
    return makeSingleItemEntity(m_itemFactory->createSeparatorItem());
}

AbstractEntityPtr ResourceTreeEntityBuilder::createLayoutItemListEntity(
    const QnResourcePtr& layoutResource) const
{
    if (!layoutResource || layoutResource->hasFlags(Qn::removed))
        return {};

    if (!layoutResource->hasFlags(Qn::layout))
        return {};

    const auto layout = layoutResource.staticCast<QnLayoutResource>();

    return std::make_unique<LayoutItemListEntity>(layout,
        layoutItemCreator(m_itemFactory.get(), userGlobalPermissions(), layout));
}

AbstractEntityPtr ResourceTreeEntityBuilder::createLayoutsGroupEntity() const
{
    auto layoutsGroupList = makeUniqueKeyGroupList<QnResourcePtr>(
        resourceItemCreator(m_itemFactory.get(), userGlobalPermissions()),
        [this](const QnResourcePtr& resource) { return createLayoutItemListEntity(resource); },
        layoutsOrder());
    layoutsGroupList->installItemSource(m_itemKeySourcePool->layoutsSource(user()));

    auto cloudLayoutItemCreator =
        [this](const QnResourcePtr& resource)
        {
            auto layout = resource.dynamicCast<QnLayoutResource>();
            NX_ASSERT(layout);
            return m_itemFactory->createCloudLayoutItem(layout);
        };

    auto layoutsComposition = std::make_unique<CompositionEntity>();
    layoutsComposition->addSubEntity(std::move(layoutsGroupList));

    if (ini().crossSystemLayouts)
    {
        auto cloudLayoutsList = makeUniqueKeyGroupList<QnResourcePtr>(
            cloudLayoutItemCreator,
            [this](const QnResourcePtr& layout) { return createLayoutItemListEntity(layout); },
            layoutsOrder());
        cloudLayoutsList->installItemSource(m_itemKeySourcePool->cloudLayoutsSource());
        layoutsComposition->addSubEntity(std::move(cloudLayoutsList));
    }

    return makeFlatteningGroup(
        m_itemFactory->createLayoutsItem(),
        std::move(layoutsComposition),
        FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createShowreelsGroupEntity() const
{
    const auto showreelItemCreator =
        [this](const QnUuid& id) { return m_itemFactory->createShowreelItem(id); };

    const auto layoutTourManager = m_commonModule->layoutTourManager();

    return makeFlatteningGroup(
        m_itemFactory->createShowreelsItem(),
        std::make_unique<ShowreelsListEntity>(showreelItemCreator, layoutTourManager),
        FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createWebPagesGroupEntity() const
{
    auto webPagesList = makeKeyList<QnResourcePtr>(
        resourceItemCreator(m_itemFactory.get(), userGlobalPermissions()),
        numericOrder());

    webPagesList->installItemSource(m_itemKeySourcePool->webPagesSource(
        user(), /*includeProxiedWebPages*/ false));

    Qt::ItemFlags flags = {Qt::ItemIsEnabled, Qt::ItemIsSelectable};

    if (userGlobalPermissions().testFlag(GlobalPermission::admin))
        flags |= Qt::ItemIsDropEnabled;

    return makeFlatteningGroup(m_itemFactory->createWebPagesItem(flags), std::move(webPagesList));
}

AbstractEntityPtr ResourceTreeEntityBuilder::createLocalFilesGroupEntity() const
{
    auto fileLayoutsGroupList = makeUniqueKeyGroupList<QnResourcePtr>(
        resourceItemCreator(m_itemFactory.get(), userGlobalPermissions()),
        [this](const QnResourcePtr& resource) { return createLayoutItemListEntity(resource); },
        numericOrder());

    fileLayoutsGroupList->installItemSource(m_itemKeySourcePool->fileLayoutsSource());

    auto mediaFilesList = makeKeyList<QnResourcePtr>(
        resourceItemCreator(m_itemFactory.get(), userGlobalPermissions()),
        locaResourcesOrder());

    mediaFilesList->installItemSource(m_itemKeySourcePool->localMediaSource());

    auto localResourceComposition = std::make_unique<CompositionEntity>();
    localResourceComposition->addSubEntity(std::move(fileLayoutsGroupList));
    localResourceComposition->addSubEntity(std::move(mediaFilesList));

    return makeFlatteningGroup(m_itemFactory->createLocalFilesItem(),
        std::move(localResourceComposition));
}

AbstractEntityPtr ResourceTreeEntityBuilder::createUsersGroupEntity() const
{
    auto userExpander =
        [this](const QnResourcePtr& resource)
        {
            return createSubjectResourcesEntity(resource.staticCast<QnUserResource>());
        };

    auto userRoleExpander =
        [this](const QnUuid& roleId)
        {
            auto roleUserExpander =
                [this](const QnResourcePtr& resource)
                {
                    return createSubjectLayoutsEntity(resource.staticCast<QnUserResource>());
                };

            auto roleUsersList = makeUniqueKeyGroupList<QnResourcePtr>(
                resourceItemCreator(m_itemFactory.get(), userGlobalPermissions()),
                roleUserExpander,
                numericOrder());

            roleUsersList->installItemSource(m_itemKeySourcePool->roleUsersSource(roleId));

            auto roleUsersResources = createSubjectResourcesEntity(
                m_commonModule->userRolesManager()->userRole(roleId));

            auto roleUsersComposition = std::make_unique<CompositionEntity>();
            roleUsersComposition->addSubEntity(std::move(roleUsersResources));
            roleUsersComposition->addSubEntity(std::move(roleUsersList));
            return roleUsersComposition;
        };

    auto rolesGroupList = makeUniqueKeyGroupList<QnUuid>(
        [this](const QnUuid& roleId) { return m_itemFactory->createUserRoleItem(roleId); },
        userRoleExpander,
        numericOrder());

    rolesGroupList->installItemSource(m_itemKeySourcePool->userRolesSource());

    auto plainUsersList = makeUniqueKeyGroupList<QnResourcePtr>(
        resourceItemCreator(m_itemFactory.get(), userGlobalPermissions()),
        userExpander,
        layoutsOrder(),
        {Qn::GlobalPermissionsRole});

    plainUsersList->installItemSource(m_itemKeySourcePool->usersSource(user()));

    auto usersComposition = std::make_unique<CompositionEntity>();
    usersComposition->addSubEntity(std::move(rolesGroupList));
    usersComposition->addSubEntity(std::move(plainUsersList));

    return makeFlatteningGroup(
        m_itemFactory->createUsersItem(),
        std::move(usersComposition),
        FlatteningGroupEntity::AutoFlatteningPolicy::noPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createVideowallsEntity() const
{
    auto videowallExpander =
        [this](const QnResourcePtr& resource)
        {
            const auto videoWall = resource.dynamicCast<QnVideoWallResource>();

            const auto videoWallScreenCreator =
                [this, videoWall](const QnUuid& id)
                {
                    return m_itemFactory->createVideoWallScreenItem(videoWall, id);
                };

            const auto videoWallMatrixCreator =
                [this, videoWall](const QnUuid& id)
                {
                    return m_itemFactory->createVideoWallMatrixItem(videoWall, id);
                };

            auto childComposition = std::make_unique<CompositionEntity>();

            childComposition->addSubEntity(
                std::make_unique<VideoWallScreensEntity>(videoWallScreenCreator, videoWall));
            childComposition->addSubEntity(
                std::make_unique<VideowallMatricesEntity>(videoWallMatrixCreator, videoWall));

            return childComposition;
        };

    auto videowallList = makeUniqueKeyGroupList<QnResourcePtr>(
        resourceItemCreator(m_itemFactory.get(), userGlobalPermissions()),
        videowallExpander,
        numericOrder());

    videowallList->installItemSource(m_itemKeySourcePool->videoWallsSource(user()));

    return videowallList;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createLocalOtherSystemsEntity() const
{
    using GroupingRule = GroupingRule<QString, QnResourcePtr>;
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    const auto systemGetter =
        [](const QnResourcePtr& resource, int/* order*/)
        {
            const auto server = resource.staticCast<QnFakeMediaServerResource>();
            const auto moduleInformation = server->getModuleInformation();

            return moduleInformation.isNewSystem()
                ? tr("New System")
                : moduleInformation.systemName;
        };

    const auto systemItemCreator =
        [this](const QString& systemName) -> AbstractItemPtr
        {
            return m_itemFactory->createOtherSystemItem(systemName);
        };

    const GroupingRule otherSystemsGroupingRule =
        {systemGetter,
        systemItemCreator,
        Qn::UuidRole,
        1, //< Dimension
        numericOrder()};

    auto otherSystemsGroupingEntity = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
        resourceItemCreator(m_itemFactory.get(), userGlobalPermissions()),
        Qn::ResourceRole,
        numericOrder(),
        GroupingRuleStack{otherSystemsGroupingRule});

    otherSystemsGroupingEntity->installItemSource(m_itemKeySourcePool->fakeServerResourcesSource());

    return otherSystemsGroupingEntity;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createCloudOtherSystemsEntity() const
{
    auto systemItemCreator =
        [this](const QString& systemId) -> AbstractItemPtr
        {
            return m_itemFactory->createCloudSystemItem(systemId);
        };

    auto cameraListCreator =
        [this](const QString& systemId) -> AbstractEntityPtr
        {
            return createCloudSystemCamerasEntity(systemId);
        };

    if (ini().crossSystemLayouts)
    {
        auto cloudSystemsEntity =
            makeUniqueKeyGroupList<QString>(systemItemCreator, cameraListCreator, numericOrder());

        cloudSystemsEntity->installItemSource(m_itemKeySourcePool->cloudSystemsSource());
        return cloudSystemsEntity;
    }
    else
    {
        auto cloudSystemsEntity = makeKeyList<QString>(systemItemCreator, numericOrder());
        cloudSystemsEntity->installItemSource(m_itemKeySourcePool->cloudSystemsSource());
        return cloudSystemsEntity;
    }
}

AbstractEntityPtr ResourceTreeEntityBuilder::createCloudSystemCamerasEntity(
    const QString& systemId) const
{
    auto itemCreator =
        [this](const QnResourcePtr& camera)
        {
            return std::make_unique<CloudCrossSystemCameraDecorator>(
                m_itemFactory->createResourceItem(camera));
        };

    auto list = makeKeyList<QnResourcePtr>(itemCreator, numericOrder());
    list->installItemSource(m_itemKeySourcePool->cloudSystemCamerasSource(systemId));

    return makeFlatteningGroup(
        m_itemFactory->createCloudSystemStatusItem(systemId),
        std::move(list),
        FlatteningGroupEntity::AutoFlatteningPolicy::headItemControl);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createOtherSystemsGroupEntity() const
{
    auto otherSystemsComposition = std::make_unique<CompositionEntity>();
    otherSystemsComposition->addSubEntity(createCloudOtherSystemsEntity());
    if (user() && user()->isOwner())
        otherSystemsComposition->addSubEntity(createLocalOtherSystemsEntity());

    return makeFlatteningGroup(
        m_itemFactory->createOtherSystemsItem(),
        std::move(otherSystemsComposition),
        FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createSubjectResourcesEntity(
    const QnResourceAccessSubject& subject) const
{
    auto userResourcesGroupContents = std::make_unique<CompositionEntity>();

    userResourcesGroupContents->addSubEntity(createSubjectDevicesEntity(subject));
    userResourcesGroupContents->addSubEntity(createSubjectLayoutsEntity(subject));

    return userResourcesGroupContents;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createSubjectDevicesEntity(
    const QnResourceAccessSubject& subject) const
{
    using GroupingRule = GroupingRule<QString, QnResourcePtr>;
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    const auto globalPermissionsManager = m_commonModule->globalPermissionsManager();
    if (globalPermissionsManager->hasGlobalPermission(subject, GlobalPermission::accessAllMedia))
        return makeSingleItemEntity(m_itemFactory->createAllCamerasAndResourcesItem());

    const auto recorderGroupCreator = recorderGroupItemCreator(m_itemFactory.get(),
        m_recorderItemDataHelper, userGlobalPermissions());

    const GroupingRule recordersGroupingRule =
        {recorderCameraGroupIdGetter(),
        recorderGroupCreator,
        Qn::CameraGroupIdRole,
        1, //< Dimension.
        numericOrder()};

    auto devicesList = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
        sharedResourceItemCreator(m_itemFactory.get(), userGlobalPermissions()),
        Qn::ResourceRole,
        layoutItemsOrder(),
        GroupingRuleStack{recordersGroupingRule});

    devicesList->installItemSource(m_itemKeySourcePool->userAccessibleDevicesSource(subject));

    return makeFlatteningGroup(
        m_itemFactory->createSharedResourcesItem(),
        std::move(devicesList),
        FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createSubjectLayoutsEntity(
    const QnResourceAccessSubject& subject) const
{
    auto layoutsList = makeUniqueKeyGroupList<QnResourcePtr>(
        subjectLayoutItemCreator(m_itemFactory.get(), userGlobalPermissions()),
        [this](const QnResourcePtr& resource) { return createLayoutItemListEntity(resource); },
        layoutsOrder());

    if (subject.isRole())
    {
        layoutsList->installItemSource(m_itemKeySourcePool->directlySharedLayoutsSource(subject));

        return makeFlatteningGroup(
            m_itemFactory->createSharedLayoutsItem(/*useRegularAppearance*/ false),
            std::move(layoutsList),
            FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy);
    }

    if (!subject.user()->userRoleIds().empty())
    {
        layoutsList->installItemSource(m_itemKeySourcePool->userLayoutsSource(subject.user()));
        return layoutsList;
    }

    if (subject.user()->userRole() == Qn::UserRole::customPermissions)
    {
        layoutsList->installItemSource(
            m_itemKeySourcePool->sharedAndOwnLayoutsSource(subject));

        return makeFlatteningGroup(
            m_itemFactory->createSharedLayoutsItem(/*useRegularAppearance*/ true),
            std::move(layoutsList),
            FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy);
    }

    const auto globalPermissionsManager = m_commonModule->globalPermissionsManager();
    if (globalPermissionsManager->hasGlobalPermission(subject, GlobalPermission::admin))
    {
        layoutsList->installItemSource(m_itemKeySourcePool->userLayoutsSource(subject.user()));
        auto adminLayoutsComposition = std::make_unique<CompositionEntity>();
        adminLayoutsComposition->addSubEntity(
            makeSingleItemEntity(m_itemFactory->createAllSharedLayoutsItem()));
        adminLayoutsComposition->addSubEntity(std::move(layoutsList));

        return adminLayoutsComposition;
    }

    layoutsList->installItemSource(
        m_itemKeySourcePool->sharedAndOwnLayoutsSource(subject));

    return layoutsList;
}

AbstractEntityPtr ResourceTreeEntityBuilder::addPinnedItem(
    AbstractEntityPtr baseEntity,
    AbstractItemPtr pinnedItem) const
{
    auto result = std::make_unique<CompositionEntity>();
    result->addSubEntity(createSpacerEntity());
    result->addSubEntity(makeSingleItemEntity(std::move(pinnedItem)));
    result->addSubEntity(createSeparatorEntity());
    result->addSubEntity(std::move(baseEntity));
    return result;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
