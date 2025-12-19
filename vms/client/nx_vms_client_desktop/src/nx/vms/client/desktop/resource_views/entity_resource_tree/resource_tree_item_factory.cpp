// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_item_factory.h"

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/resource_property_key.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/resource_views/entity_item_model/item/generic_item/generic_item_builder.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/item/generic_item_data.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/recorder_item_data_helper.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/core/skin/resource_icon_cache.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/other_servers/other_servers_manager.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/cloud_system_status_item.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/resource_item.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/showreel_item.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/videowall_matrix_item.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/videowall_screen_item.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>

namespace {

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::core::entity_item_model;
using namespace nx::vms::client::desktop::entity_resource_tree;

using nx::vms::common::SystemSettings;
using nx::vms::client::core::CloudCrossSystemContext;
using nx::vms::client::core::entity_resource_tree::RecorderItemDataHelper;
using nx::vms::client::core::ResourceIconCache;

//-------------------------------------------------------------------------------------------------
// Provider and invalidator pair factory functions for the header system name generic item.
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
        context->systemDescription().get(),
        &nx::vms::client::core::SystemDescription::systemNameChanged,
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
                    ResourceIconCache::CloudSystem | ResourceIconCache::Offline);
            }

            const auto status = context->status();
            if (status == CloudCrossSystemContext::Status::connectionFailure)
            {
                return static_cast<int>(
                    ResourceIconCache::CloudSystem | ResourceIconCache::Locked);
            }

            if (status == CloudCrossSystemContext::Status::unsupportedTemporary)
            {
                return static_cast<int>(
                    ResourceIconCache::CloudSystem | ResourceIconCache::Incompatible);
            }

            return static_cast<int>(ResourceIconCache::CloudSystem);
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
        context->systemDescription().get(),
        &nx::vms::client::core::SystemDescription::onlineStateChanged,
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

            return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
        };
}

GenericItem::DataProvider otherServerNameProvider(const nx::Uuid& serverId, SystemContext* systemContext)
{
    return [serverId, systemContext]
        {
            return systemContext->otherServersManager()
                ->getModuleInformationWithAddresses(serverId).name;
        };
}

InvalidatorPtr otherServerNameInvalidator(const nx::Uuid& id, SystemContext* systemContext)
{
    auto result = std::make_shared<Invalidator>();

    result->connections()->add(QObject::connect(
        systemContext->otherServersManager(),
        &OtherServersManager::serverUpdated,
        [invalidator = result.get(), id](const nx::Uuid& serverId)
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
            return static_cast<int>(ResourceIconCache::Server);
        };
}

InvalidatorPtr otherServerIconInvalidator()
{
    return std::make_shared<Invalidator>();
}

} // namespace

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::vms::client::core::entity_resource_tree;
using NodeType = nx::vms::client::core::ResourceTree::NodeType;

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
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::CurrentSystem))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::currentSystem))
        .withFlags({Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemNeverHasChildren});
}

AbstractItemPtr ResourceTreeItemFactory::createCurrentUserItem(const QnUserResourcePtr& user) const
{
    const auto nameProvider = resourceNameProvider(user);
    const auto nameInvalidator = resourceNameInvalidator(user);

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, nameProvider, nameInvalidator)
        .withRole(core::ResourceRole, QVariant::fromValue(user.staticCast<QnResource>()))
        .withRole(
            core::ResourceIconKeyRole,
            static_cast<int>(appContext()->resourceIconCache()->userKey(user)))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::currentUser))
        .withRole(core::ExtraInfoRole, QnResourceDisplayInfo(user).extraInfo())
        .withFlags({Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemNeverHasChildren});
}

AbstractItemPtr ResourceTreeItemFactory::createSpacerItem() const
{
    return GenericItemBuilder()
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::spacer))
        .withFlags({Qt::ItemNeverHasChildren});
}

AbstractItemPtr ResourceTreeItemFactory::createSeparatorItem() const
{
    return GenericItemBuilder()
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::separator))
        .withFlags({Qt::ItemNeverHasChildren});
}

AbstractItemPtr ResourceTreeItemFactory::createServersItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Servers"))
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::Servers))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::servers))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MainWindow_Tree_Servers));
}

AbstractItemPtr ResourceTreeItemFactory::createHealthMonitorsItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Health Monitors"))
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::HealthMonitors))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::healthMonitors));
}

AbstractItemPtr ResourceTreeItemFactory::createVideoWallsItem(QString customName) const
{
    const QString name = customName.isEmpty() ? tr("Video Walls") : customName;

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, name)
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::VideoWall))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::videoWalls));
}

AbstractItemPtr ResourceTreeItemFactory::createCamerasAndDevicesItem(Qt::ItemFlags flags) const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Cameras & Devices"))
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::Cameras))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::camerasAndDevices))
        .withFlags(flags);
}

AbstractItemPtr ResourceTreeItemFactory::createLayoutsItem(Qt::ItemFlags flags) const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Layouts"))
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::Layouts))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::layouts))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MainWindow_Tree_Layouts))
        .withFlags(flags);
}

AbstractItemPtr ResourceTreeItemFactory::createShowreelsItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Showreels"))
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::Showreels))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::showreels))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::Showreel));
}

AbstractItemPtr ResourceTreeItemFactory::createIntegrationsItem(Qt::ItemFlags flags) const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Integrations"))
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::Integrations))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::integrations))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MainWindow_Tree_WebPage))
        .withFlags(flags);
}

AbstractItemPtr ResourceTreeItemFactory::createWebPagesItem(Qt::ItemFlags flags) const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Web Pages"))
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::WebPages))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::webPages))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MainWindow_Tree_WebPage))
        .withFlags(flags);
}

AbstractItemPtr ResourceTreeItemFactory::createWebPagesAndIntegrationsItem(
    Qt::ItemFlags flags) const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Web Pages & Integrations"))
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::WebPages))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::webPages))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MainWindow_Tree_WebPage))
        .withFlags(flags);
}

AbstractItemPtr ResourceTreeItemFactory::createUsersItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Users"))
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::Users))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::users))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MainWindow_Tree_Users));
}

AbstractItemPtr ResourceTreeItemFactory::createOtherSystemsItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Other Sites"))
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::OtherSystems))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::otherSystems))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::OtherSystems));
}

AbstractItemPtr ResourceTreeItemFactory::createLocalFilesItem() const
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, tr("Local Files"))
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::LocalResources))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::localResources))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MediaFolders));
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
    using namespace nx::vms::client::core::entity_resource_tree::resource_grouping;

    const auto displayText = getResourceTreeDisplayText(compositeGroupId);

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, displayText)
        .withRole(Qt::ToolTipRole, displayText)
        .withRole(Qt::EditRole, displayText)
        .withRole(core::ResourceTreeCustomGroupIdRole, compositeGroupId)
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::LocalResources))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::customResourceGroup))
        .withFlags(itemFlags);
}

AbstractItemPtr ResourceTreeItemFactory::createRecorderItem(
    const QString& cameraGroupId,
    const QSharedPointer<core::entity_resource_tree::RecorderItemDataHelper>& recorderItemDataHelper,
    Qt::ItemFlags itemFlags)
{
    const auto nameProvider = recorderNameProvider(cameraGroupId, recorderItemDataHelper);
    const auto nameInvalidator = recorderNameInvalidator(cameraGroupId, recorderItemDataHelper);
    const auto iconProvider = recorderIconProvider(cameraGroupId, recorderItemDataHelper);
    const auto iconInvalidator = recorderIconInvalidator(cameraGroupId, recorderItemDataHelper);

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, nameProvider, nameInvalidator)
        .withRole(Qt::ToolTipRole, nameProvider, nameInvalidator)
        .withRole(Qt::EditRole, nameProvider, nameInvalidator)
        .withRole(core::ResourceIconKeyRole, iconProvider, iconInvalidator)
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::recorder))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::MainWindow_Tree_Recorder))
        .withRole(core::CameraGroupIdRole, cameraGroupId)
        .withFlags(itemFlags);
}

AbstractItemPtr ResourceTreeItemFactory::createOtherSystemItem(const QString& systemId)
{
    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, systemId)
        .withRole(core::ResourceIconKeyRole, static_cast<int>(ResourceIconCache::OtherSystem))
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::localSystem))
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
        .withRole(core::ResourceIconKeyRole, iconProvider, iconInvalidator)
        .withRole(Qn::CloudSystemIdRole, systemId)
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::cloudSystem))
        .withRole(Qn::HelpTopicIdRole, static_cast<int>(HelpTopic::Id::OtherSystems))
        .withRole(core::ExtraInfoRole, extraInfoProvider, extraInfoInvalidator)
        .withFlags(cloudSystemFlagsProvider(systemId));
}

AbstractItemPtr ResourceTreeItemFactory::createShowreelItem(const nx::Uuid& showreelId)
{
    return std::make_unique<ShowreelItem>(systemContext()->showreelManager(), showreelId);
}

AbstractItemPtr ResourceTreeItemFactory::createVideoWallScreenItem(
    const QnVideoWallResourcePtr& videoWall,
    const nx::Uuid& screenUuid)
{
    return std::make_unique<VideoWallScreenItem>(videoWall, screenUuid);
}

AbstractItemPtr ResourceTreeItemFactory::createVideoWallMatrixItem(
    const QnVideoWallResourcePtr& videoWall,
    const nx::Uuid& matrixUuid)
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
    const auto groupIdProvider = cloudLayoutGroupIdProvider(layout);
    const auto groupIdInvalidator = cloudLayoutGroupIdInvalidator(layout);

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, nameProvider, nameInvalidator)
        .withRole(core::ResourceRole, QVariant::fromValue(layout.staticCast<QnResource>()))
        .withRole(core::ResourceIconKeyRole, iconProvider, iconInvalidator)
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::resource))
        .withRole(core::ResourceExtraStatusRole, extraStatusProvider, extraStatusInvalidator)
        .withRole(core::ResourceTreeCustomGroupIdRole, groupIdProvider, groupIdInvalidator)
        .withFlags(flagsProvider);
}

AbstractItemPtr ResourceTreeItemFactory::createOtherServerItem(const nx::Uuid& serverId)
{
    const auto nameProvider = otherServerNameProvider(serverId, systemContext());
    const auto nameInvalidator = otherServerNameInvalidator(serverId, systemContext());

    return GenericItemBuilder()
        .withRole(Qt::DisplayRole, nameProvider, nameInvalidator)
        .withRole(core::ResourceIconKeyRole, otherServerIconProvider(), otherServerIconInvalidator())
        .withRole(core::NodeTypeRole, QVariant::fromValue(NodeType::otherSystemServer))
        .withRole(Qn::ItemUuidRole, QVariant::fromValue(serverId))
        .withRole(Qn::HelpTopicIdRole, HelpTopic::Id::OtherSystems);
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
