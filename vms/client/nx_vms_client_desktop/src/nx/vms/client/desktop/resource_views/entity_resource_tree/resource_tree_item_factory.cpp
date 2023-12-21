// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_item_factory.h"

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <finders/systems_finder.h>
#include <network/base_system_description.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/other_servers/other_servers_manager.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/generic_item/generic_item_builder.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/cloud_system_status_item.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/resource_item.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/showreel_item.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/videowall_matrix_item.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/videowall_screen_item.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/recorder_item_data_helper.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>

namespace {

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::entity_item_model;
using namespace nx::vms::client::desktop::entity_resource_tree;
using nx::vms::common::SystemSettings;

//-------------------------------------------------------------------------------------------------
// Provider and invalidator pair factory functions for the header system name name generic item.
//-------------------------------------------------------------------------------------------------
GenericItem::DataProvider systemNameProvider(const SystemSettings* globalSettings)
{
    return [globalSettings] { return globalSettings->systemName(); };
}

InvalidatorPtr systemNameInvalidator(const SystemSettings* globalSettings)
{
    auto result = std::make_shared<Invalidator>();

    result->connections()->add(globalSettings->connect(
        globalSettings, &SystemSettings::systemNameChanged, globalSettings,
        [invalidator = result.get()] { invalidator->invalidate(); }));

    return result;
}

//-------------------------------------------------------------------------------------------------
// Provider and invalidator pair factory functions for the header resource name generic item.
//-------------------------------------------------------------------------------------------------
GenericItem::DataProvider resourceNameProvider(const QnResourcePtr& resource)
{
    return [resource] { return resource->getName(); };
}

InvalidatorPtr resourceNameInvalidator(const QnResourcePtr& resource)
{
    auto result = std::make_shared<Invalidator>();

    result->connections()->add(resource->connect(
        resource.get(), &QnResource::nameChanged,
        [invalidator = result.get()] { invalidator->invalidate(); }));

    return result;
}

//-------------------------------------------------------------------------------------------------
// Provider and invalidator pair factory functions for a recorder group name generic item.
//-------------------------------------------------------------------------------------------------
GenericItem::DataProvider recorderNameProvider(
    const QString& groupId,
    const QSharedPointer<RecorderItemDataHelper>& recorderItemDataHelper)
{
    return [groupId, recorderItemDataHelper]
    {
        return recorderItemDataHelper->groupedDeviceName(groupId);
    };
}

InvalidatorPtr recorderNameInvalidator(
    const QString& groupId,
    const QSharedPointer<RecorderItemDataHelper>& recorderItemDataHelper)
{
    auto result = std::make_shared<Invalidator>();

    result->connections()->add(recorderItemDataHelper->connect(
        recorderItemDataHelper.get(), &RecorderItemDataHelper::groupedDeviceNameChanged,
        [groupId, invalidator = result.get()](const QString& changedGroupId)
        {
            if (groupId == changedGroupId)
                invalidator->invalidate();
        }));

    return result;
}

//-------------------------------------------------------------------------------------------------
// Icon data provider / invalidator and flags provider factory functions for the cloud system item.
//-------------------------------------------------------------------------------------------------

GenericItem::DataProvider cloudSystemNameProvider(const QString& systemId)
{
    return
        [systemId]
        {
            auto context = appContext()->cloudCrossSystemManager()->systemContext(systemId);

            if (context)
                return context->systemDescription()->name();

            return QString{};
        };
}

InvalidatorPtr cloudSystemNameInvalidator(const QString& systemId)
{
    const auto context = appContext()->cloudCrossSystemManager()->systemContext(systemId);
    auto result = std::make_shared<Invalidator>();

    if (!context)
        return result;

    result->connections()->add(QObject::connect(
        context->systemDescription(),
        &QnBaseSystemDescription::systemNameChanged,
        [invalidator = result.get()]
        {
            invalidator->invalidate();
        }));

    return result;
}

GenericItem::DataProvider cloudSystemIconProvider(const QString& systemId)
{
    return
        [systemId]
        {
            auto context = appContext()->cloudCrossSystemManager()->systemContext(systemId);
            if (!context
                || !context->systemDescription()->isOnline()
                || context->status() == CloudCrossSystemContext::Status::uninitialized)
            {
                return static_cast<int>(
                    QnResourceIconCache::CloudSystem | QnResourceIconCache::Offline);
            }

            const auto status = context->status();
            if (status == CloudCrossSystemContext::Status::connectionFailure)
            {
                return static_cast<int>(
                    QnResourceIconCache::CloudSystem | QnResourceIconCache::Locked);
            }

            if (status == CloudCrossSystemContext::Status::unsupportedTemporary)
            {
                return static_cast<int>(
                    QnResourceIconCache::CloudSystem | QnResourceIconCache::Incompatible);
            }

            return static_cast<int>(QnResourceIconCache::CloudSystem);
        };
}

InvalidatorPtr cloudSystemIconInvalidator(const QString& systemId)
{
    const auto context = appContext()->cloudCrossSystemManager()->systemContext(systemId);
    auto result = std::make_shared<Invalidator>();

    if (!context)
        return result;

    const auto invalidate = [invalidator = result.get()] { invalidator->invalidate(); };

    result->connections()->add(QObject::connect(
        context,
        &CloudCrossSystemContext::statusChanged,
        invalidate));

    result->connections()->add(QObject::connect(
        context->systemDescription(),
        &QnBaseSystemDescription::onlineStateChanged,
        invalidate));

    return result;
}

GenericItem::DataProvider cloudSystemExtraInfoProvider(const QString& systemId)
{
    return
        [systemId]
        {
            auto context = appContext()->cloudCrossSystemManager()->systemContext(systemId);
            if (context
                && context->status() == CloudCrossSystemContext::Status::unsupportedPermanently)
            {
                return context->systemDescription()->version().toString();
            }

            return QString{};
        };
}

InvalidatorPtr cloudSystemExtraInfoInvalidator(const QString& systemId)
{
    const auto context = appContext()->cloudCrossSystemManager()->systemContext(systemId);
    auto result = std::make_shared<Invalidator>();

    if (!context)
        return result;

    const auto invalidate = [invalidator = result.get()] { invalidator->invalidate(); };

    result->connections()->add(QObject::connect(
        context,
        &CloudCrossSystemContext::statusChanged,
        invalidate));

    return result;
}

GenericItem::FlagsProvider cloudSystemFlagsProvider(const QString& systemId)
{
    return
        [systemId]() -> Qt::ItemFlags
        {
            auto context = appContext()->cloudCrossSystemManager()->systemContext(systemId);
            if (!context)
                return {};

            if (context->systemDescription()->isOnline()
                && context->status() != CloudCrossSystemContext::Status::uninitialized)
            {
                return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
            }

            return Qt::ItemIsSelectable;
        };
}

//-------------------------------------------------------------------------------------------------
// Icon data provider / invalidator and flags provider factory functions for the cloud layout item.
//-------------------------------------------------------------------------------------------------
GenericItem::DataProvider cloudLayoutIconProvider(const QnLayoutResourcePtr& layout)
{
    return
        [layout]
        {
            return layout->locked()
                ? static_cast<int>(QnResourceIconCache::CloudLayout | QnResourceIconCache::Locked)
                : static_cast<int>(QnResourceIconCache::CloudLayout);
        };
}

InvalidatorPtr cloudLayoutIconInvalidator(const QnLayoutResourcePtr& layout)
{
    auto result = std::make_shared<Invalidator>();

    result->connections()->add(QObject::connect(
        layout.get(),
        &QnLayoutResource::lockedChanged,
        [invalidator = result.get()]
        {
            invalidator->invalidate();
        }));

    return result;
}

GenericItem::DataProvider cloudLayoutExtraStatusProvider(const QnLayoutResourcePtr& layout)
{
    return
        [layout]
        {
            return layout->locked()
                ? static_cast<int>(ResourceExtraStatusFlag::locked)
                : static_cast<int>(ResourceExtraStatusFlag::empty);
        };
}

InvalidatorPtr cloudLayoutExtraStatusInvalidator(const QnLayoutResourcePtr& layout)
{
    auto result = std::make_shared<Invalidator>();

    result->connections()->add(QObject::connect(
        layout.get(),
        &QnLayoutResource::lockedChanged,
        [invalidator = result.get()]
        {
            invalidator->invalidate();
        }));

    return result;
}

GenericItem::FlagsProvider cloudLayoutFlagsProvider(const QnLayoutResourcePtr& layout)
{
    return
        [weakCloudLayout = layout.toWeakRef()]() -> Qt::ItemFlags
        {
            if (auto cloudLayout = weakCloudLayout.lock())
            {
                if (!cloudLayout->locked()
                    && cloudLayout->getStatus() == nx::vms::api::ResourceStatus::online)
                {
                    return Qt::ItemIsEnabled
                        | Qt::ItemIsSelectable
                        | Qt::ItemIsEditable
                        | Qt::ItemIsDragEnabled
                        | Qt::ItemIsDropEnabled;
                }
            }

            return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        };
}

GenericItem::DataProvider otherServerNameProvider(const QnUuid& serverId, SystemContext* systemContext)
{
    return [serverId, systemContext]
        {
            return systemContext->otherServersManager()
                ->getModuleInformationWithAddresses(serverId).name;
        };
}

InvalidatorPtr otherServerNameInvalidator(const QnUuid& id, SystemContext* systemContext)
{
    auto result = std::make_shared<Invalidator>();

    result->connections()->add(QObject::connect(
        systemContext->otherServersManager(),
        &OtherServersManager::serverUpdated,
        [invalidator = result.get(), id](const QnUuid& serverId)
        {
            if (id == serverId)
                invalidator->invalidate();
        }));

    return result;
}

GenericItem::DataProvider otherServerIconProvider()
{
    return []
        {
            return static_cast<int>(QnResourceIconCache::Server);
        };
}

InvalidatorPtr otherServerIconInvalidator()
{
    return std::make_shared<Invalidator>();
}

} // namespace

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using NodeType = ResourceTree::NodeType;
using IconCache = QnResourceIconCache;

using namespace entity_item_model;

ResourceTreeItemFactory::ResourceTreeItemFactory(SystemContext* systemContext):
    base_type(),
    SystemContextAware(systemContext)
{
}

AbstractItemPtr ResourceTreeItemFactory::createCurrentSystemItem() const
{
    const auto nameProvider = systemNameProvider(systemSettings());
    const auto nameInvalidator = systemNameInvalidator(systemSettings());

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, nameProvider, nameInvalidator)
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::CurrentSystem))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::currentSystem))
        .withFlags({Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemNeverHasChildren});
}

AbstractItemPtr ResourceTreeItemFactory::createCurrentUserItem(const QnUserResourcePtr& user) const
{
    const auto nameProvider = resourceNameProvider(user);
    const auto nameInvalidator = resourceNameInvalidator(user);

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, nameProvider, nameInvalidator)
        .withRole(Qn::ResourceRole, QVariant::fromValue(user.staticCast<QnResource>()))
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::userKey(user)))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::currentUser))
        .withRole(Qn::ExtraInfoRole, QnResourceDisplayInfo(user).extraInfo())
        .withFlags({Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemNeverHasChildren});
}

AbstractItemPtr ResourceTreeItemFactory::createSpacerItem() const
{
    return GenericItemBuilder()
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::spacer))
        .withFlags({Qt::ItemNeverHasChildren});
}

AbstractItemPtr ResourceTreeItemFactory::createSeparatorItem() const
{
    return GenericItemBuilder()
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::separator))
        .withFlags({Qt::ItemNeverHasChildren});
}

AbstractItemPtr ResourceTreeItemFactory::createServersItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Servers"))
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::Servers))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::servers))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MainWindow_Tree_Servers));
}

AbstractItemPtr ResourceTreeItemFactory::createHealthMonitorsItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Health Monitors"))
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::Servers))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::servers));
}

AbstractItemPtr ResourceTreeItemFactory::createVideoWallsItem(QString customName) const
{
    const QString name = customName.isEmpty() ? tr("Video Walls") : customName;

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, name)
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::VideoWall))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::videoWalls));
}

AbstractItemPtr ResourceTreeItemFactory::createCamerasAndDevicesItem(Qt::ItemFlags itemFlags) const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Cameras & Devices"))
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::Cameras))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::camerasAndDevices))
        .withFlags(itemFlags);
}

AbstractItemPtr ResourceTreeItemFactory::createLayoutsItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Layouts"))
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::Layouts))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::layouts))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MainWindow_Tree_Layouts));
}

AbstractItemPtr ResourceTreeItemFactory::createShowreelsItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Showreels"))
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::Showreels))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::showreels))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::Showreel));
}

AbstractItemPtr ResourceTreeItemFactory::createIntegrationsItem(Qt::ItemFlags flags) const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Integrations"))
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::Integrations))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::integrations))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MainWindow_Tree_WebPage))
        .withFlags(flags);
}

AbstractItemPtr ResourceTreeItemFactory::createWebPagesItem(Qt::ItemFlags flags) const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Web Pages"))
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::WebPages))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::webPages))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MainWindow_Tree_WebPage))
        .withFlags(flags);
}

AbstractItemPtr ResourceTreeItemFactory::createWebPagesAndIntegrationsItem(
    Qt::ItemFlags flags) const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Web Pages & Integrations"))
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::WebPages))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::webPages))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MainWindow_Tree_WebPage))
        .withFlags(flags);
}

AbstractItemPtr ResourceTreeItemFactory::createUsersItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Users"))
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::Users))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::users))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MainWindow_Tree_Users));
}

AbstractItemPtr ResourceTreeItemFactory::createOtherSystemsItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Other Systems"))
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::OtherSystems))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::otherSystems))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::OtherSystems));
}

AbstractItemPtr ResourceTreeItemFactory::createLocalFilesItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Local Files"))
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::LocalResources))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::localResources))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MediaFolders));
}

AbstractItemPtr ResourceTreeItemFactory::createAllCamerasAndResourcesItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("All Cameras & Resources"))
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::Cameras))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::allCamerasAccess))
        .withFlags({Qt::ItemNeverHasChildren});
}

AbstractItemPtr ResourceTreeItemFactory::createAllSharedLayoutsItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("All Shared Layouts"))
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::SharedLayouts))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::allLayoutsAccess))
        .withFlags({Qt::ItemNeverHasChildren});
}

AbstractItemPtr ResourceTreeItemFactory::createSharedResourcesItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Cameras & Resources"))
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::Cameras))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::sharedResources));
}

AbstractItemPtr ResourceTreeItemFactory::createSharedLayoutsItem(bool useRegularAppearance) const
{
    const auto displayText = useRegularAppearance
        ? tr("Layouts")
        : tr("Shared Layouts");

   const auto iconKey = useRegularAppearance
       ? static_cast<int>(IconCache::Layouts)
       : static_cast<int>(IconCache::SharedLayouts);

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, displayText)
        .withRole(Qn::ResourceIconKeyRole, iconKey)
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::sharedLayouts))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MainWindow_Tree_Layouts));
}

AbstractItemPtr ResourceTreeItemFactory::createResourceItem(const QnResourcePtr& resource)
{
    if (!m_resourceItemCache.contains(resource))
    {
        const auto sharedItemOrigin =
            std::make_shared<SharedItemOrigin>(std::make_unique<ResourceItem>(resource));

        sharedItemOrigin->setSharedInstanceCountObserver(
            [this, resource](int count)
            {
                if (count == 0)
                    m_resourceItemCache.remove(resource);
            });

        m_resourceItemCache.insert(resource, sharedItemOrigin);
    }
    return m_resourceItemCache.value(resource)->createSharedInstance();
}

AbstractItemPtr ResourceTreeItemFactory::createResourceGroupItem(
    const QString& compositeGroupId,
    const Qt::ItemFlags itemFlags)
{
    const auto displayText = resource_grouping::getResourceTreeDisplayText(compositeGroupId);

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, displayText)
        .withRole(Qt::ToolTipRole, displayText)
        .withRole(Qt::EditRole, displayText)
        .withRole(Qn::ResourceTreeCustomGroupIdRole, compositeGroupId)
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::LocalResources))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::customResourceGroup))
        .withFlags(itemFlags);
}

AbstractItemPtr ResourceTreeItemFactory::createRecorderItem(
    const QString& cameraGroupId,
    const QSharedPointer<RecorderItemDataHelper>& recorderItemDataHelper,
    Qt::ItemFlags itemFlags)
{
    const auto iconKey = recorderItemDataHelper->isMultisensorCamera(cameraGroupId)
        ? IconCache::MultisensorCamera
        : IconCache::Recorder;
    const auto nameProvider = recorderNameProvider(cameraGroupId, recorderItemDataHelper);
    const auto nameInvalidator = recorderNameInvalidator(cameraGroupId, recorderItemDataHelper);

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, nameProvider, nameInvalidator)
        .withRole(Qt::ToolTipRole, nameProvider, nameInvalidator)
        .withRole(Qt::EditRole, nameProvider, nameInvalidator)
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(iconKey))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::recorder))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MainWindow_Tree_Recorder))
        .withRole(Qn::CameraGroupIdRole, cameraGroupId)
        .withFlags(itemFlags);
}

AbstractItemPtr ResourceTreeItemFactory::createOtherSystemItem(const QString& systemId)
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, systemId)
        .withRole(Qn::ResourceIconKeyRole, static_cast<int>(IconCache::OtherSystem))
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::localSystem))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::OtherSystems));
}

AbstractItemPtr ResourceTreeItemFactory::createCloudSystemItem(const QString& systemId)
{
    const auto nameProvider = cloudSystemNameProvider(systemId);
    const auto nameInvalidator = cloudSystemNameInvalidator(systemId);
    const auto iconProvider = cloudSystemIconProvider(systemId);
    const auto iconInvalidator = cloudSystemIconInvalidator(systemId);
    const auto extraInfoProvider = cloudSystemExtraInfoProvider(systemId);
    const auto extraInfoInvalidator = cloudSystemExtraInfoInvalidator(systemId);

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, nameProvider, nameInvalidator)
        .withRole(Qn::ResourceIconKeyRole, iconProvider, iconInvalidator)
        .withRole(Qn::CloudSystemIdRole, systemId)
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::cloudSystem))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::OtherSystems))
        .withRole(Qn::ExtraInfoRole, extraInfoProvider, extraInfoInvalidator)
        .withFlags(cloudSystemFlagsProvider(systemId));
}

AbstractItemPtr ResourceTreeItemFactory::createShowreelItem(const QnUuid& showreelId)
{
    return std::make_unique<ShowreelItem>(systemContext()->showreelManager(), showreelId);
}

AbstractItemPtr ResourceTreeItemFactory::createVideoWallScreenItem(
    const QnVideoWallResourcePtr& videoWall,
    const QnUuid& screenUuid)
{
    return std::make_unique<VideoWallScreenItem>(videoWall, screenUuid);
}

AbstractItemPtr ResourceTreeItemFactory::createVideoWallMatrixItem(
    const QnVideoWallResourcePtr& videoWall,
    const QnUuid& matrixUuid)
{
    return std::make_unique<VideoWallMatrixItem>(videoWall, matrixUuid);
}

AbstractItemPtr ResourceTreeItemFactory::createCloudSystemStatusItem(const QString& systemId)
{
    return std::make_unique<CloudSystemStatusItem>(systemId);
}

AbstractItemPtr ResourceTreeItemFactory::createCloudLayoutItem(const QnLayoutResourcePtr& layout)
{
    const auto nameProvider = resourceNameProvider(layout);
    const auto nameInvalidator = resourceNameInvalidator(layout);
    const auto iconProvider = cloudLayoutIconProvider(layout);
    const auto iconInvalidator = cloudLayoutIconInvalidator(layout);
    const auto flagsProvider = cloudLayoutFlagsProvider(layout);
    const auto extraStatusProvider = cloudLayoutExtraStatusProvider(layout);
    const auto extraStatusInvalidator = cloudLayoutExtraStatusInvalidator(layout);

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, nameProvider, nameInvalidator)
        .withRole(Qn::ResourceRole, QVariant::fromValue(layout.staticCast<QnResource>()))
        .withRole(Qn::ResourceIconKeyRole, iconProvider, iconInvalidator)
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::resource))
        .withRole(Qn::ResourceExtraStatusRole, extraStatusProvider, extraStatusInvalidator)
        .withFlags(flagsProvider);
}

AbstractItemPtr ResourceTreeItemFactory::createOtherServerItem(const QnUuid& serverId)
{
    const auto nameProvider = otherServerNameProvider(serverId, systemContext());
    const auto nameInvalidator = otherServerNameInvalidator(serverId, systemContext());

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, nameProvider, nameInvalidator)
        .withRole(Qn::ResourceIconKeyRole, otherServerIconProvider(), otherServerIconInvalidator())
        .withRole(Qn::NodeTypeRole, QVariant::fromValue(NodeType::otherSystemServer))
        .withRole(Qn::ItemUuidRole, QVariant::fromValue(serverId))
        .withRole(Qn::HelpTopicIdRole, HelpTopic::Id::OtherSystems);
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
