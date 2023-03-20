// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_layouts_handler.h"

#include <QtWidgets/QAction>

#include <client/client_globals.h>
#include <client/client_message_processor.h>
#include <client/client_settings.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/layout_reader.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/counter.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/metatypes.h>
#include <nx/utils/string.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/desktop/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/desktop/cross_system/dialogs/cloud_layouts_intro_dialog.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/radass/radass_resource_manager.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/resource/unified_resource_pool.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameter_types.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/actions/common_action.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_event_rules_manager.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <nx_ec/managers/abstract_resource_manager.h>
#include <ui/dialogs/layout_name_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/extensions/workbench_layout_change_validator.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/handlers/workbench_videowall_handler.h> //< TODO: #sivanov Dependencies.
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_state_manager.h>
#include <utils/common/delete_later.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>
#include <watchers/cloud_status_watcher.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

namespace {

QString generateUniqueLayoutName(
    QnResourcePool* resourcePool,
    const QnUserResourcePtr &user,
    const QString &defaultName,
    const QString &nameTemplate)
{
    QStringList usedNames;
    QnUuid parentId = user ? user->getId() : QnUuid();
    for (const auto layout: resourcePool->getResources<LayoutResource>())
    {
        if (layout->isShared() || layout->getParentId() == parentId)
            usedNames.push_back(layout->getName());
    }

    return nx::utils::generateUniqueString(usedNames, defaultName, nameTemplate);
}

/**
 * @brief alreadyExistingLayouts    Check if layouts with same name already exist.
 * @param name                      Suggested new name.
 * @param parentId                  Layout owner.
 * @param layout                    Layout that we want to rename (if any).
 * @return                          List of existing layouts with same name.
 */
LayoutResourceList alreadyExistingLayouts(
    QnResourcePool* resourcePool,
    const QString& name,
    const QnUuid& parentId,
    const LayoutResourcePtr& layout = LayoutResourcePtr())
{
    LayoutResourceList result;
    for (const auto& existingLayout:
        resourcePool->getResourcesByParentId(parentId).filtered<LayoutResource>())
    {
        if (existingLayout == layout)
            continue;

        if (existingLayout->getName().toLower() != name.toLower())
            continue;

        result << existingLayout;
    }
    return result;
}

QnResourceList calculateResourcesToShare(const QnResourceList& resources,
    const QnUserResourcePtr& user)
{
    auto sharingRequired = [user](const QnResourcePtr& resource)
        {
            if (!QnResourceAccessFilter::isShareableMedia(resource))
                return false;

            auto accessProvider = resource->systemContext()->resourceAccessProvider();
            return !accessProvider->hasAccess(user, resource);
        };
    return resources.filtered(sharingRequired);
}

QSet<QnResourcePtr> localLayoutResources(QnResourcePool* resourcePool,
    const QnLayoutItemDataMap& items)
{
    QSet<QnResourcePtr> result;
    for (const auto& item: items)
    {
        if (auto resource = resourcePool->getResourceByDescriptor(item.resource))
            result.insert(resource);
    }
    return result;
}

QSet<QnResourcePtr> localLayoutResources(QnResourcePool* resourcePool,
    const nx::vms::api::LayoutItemDataList& items)
{
    QSet<QnResourcePtr> result;
    for (const auto& item: items)
    {
        const nx::vms::common::ResourceDescriptor descriptor{item.resourceId, item.resourcePath};
        if (auto resource = resourcePool->getResourceByDescriptor(descriptor))
            result.insert(resource);
    }
    return result;
}

QSet<QnResourcePtr> localLayoutResources(const LayoutResourcePtr& layout)
{
    return localLayoutResources(layout->resourcePool(), layout->getItems());
}

bool isCloudLayout(const LayoutResourcePtr& layout)
{
    return layout->hasFlags(Qn::cross_system);
}

bool hasCrossSystemItems(const LayoutResourcePtr& layout)
{
    const auto currentCloudSystemId = appContext()->currentSystemContext()->globalSettings()
        ->cloudSystemId();

    auto items = layout->getItems();
    return std::any_of(items.cbegin(), items.cend(),
        [currentCloudSystemId](const QnLayoutItemData& item)
        {
            return isCrossSystemResource(item.resource)
                && crossSystemResourceSystemId(item.resource) != currentCloudSystemId;
        });
}

} // namespace

LayoutsHandler::LayoutsHandler(QObject *parent):
    QObject(parent),
    QnSessionAwareDelegate(parent)
{
    auto getCurrentLayout =
        [this]() { return workbench()->currentLayout()->resource(); };
    auto getLayoutFromParameters =
        [this]()
        {
            return menu()->currentParameters(sender()).resource().dynamicCast<LayoutResource>();
        };

    connect(action(action::NewUserLayoutAction), &QAction::triggered, this,
        &LayoutsHandler::at_newUserLayoutAction_triggered);

    connect(action(action::SaveLayoutAction), &QAction::triggered, this,
        [=]() { saveLayout(getLayoutFromParameters()); });

    connect(action(action::SaveCurrentLayoutAction), &QAction::triggered, this,
        [=]() { saveLayout(getCurrentLayout()); });

    connect(action(action::SaveLayoutAsCloudAction), &QAction::triggered, this,
        [=]() { saveLayoutAsCloud(getLayoutFromParameters()); });

    connect(action(action::SaveCurrentLayoutAsCloudAction), &QAction::triggered, this,
        [=]() { saveLayoutAsCloud(getCurrentLayout());});

    connect(action(action::SaveLayoutAsAction), &QAction::triggered, this,
        [=]() { saveLayoutAs(getLayoutFromParameters()); });

    connect(action(action::SaveCurrentLayoutAsAction), &QAction::triggered, this,
        [=]() { saveLayoutAs(getCurrentLayout()); });

    connect(action(action::CloseLayoutAction), &QAction::triggered, this,
        &LayoutsHandler::at_closeLayoutAction_triggered);
    connect(action(action::CloseAllButThisLayoutAction), &QAction::triggered, this,
        &LayoutsHandler::at_closeAllButThisLayoutAction_triggered);
    connect(action(action::RemoveFromServerAction), &QAction::triggered, this,
        &LayoutsHandler::at_removeFromServerAction_triggered);
    connect(action(action::OpenNewTabAction), &QAction::triggered, this,
        &LayoutsHandler::at_openNewTabAction_triggered);
    connect(action(action::ForgetLayoutPasswordAction), &QAction::triggered, this,
        &LayoutsHandler::at_forgetLayoutPasswordAction_triggered);

    connect(action(action::RemoveLayoutItemAction), &QAction::triggered, this,
        &LayoutsHandler::at_removeLayoutItemAction_triggered);
    connect(action(action::RemoveLayoutItemFromSceneAction), &QAction::triggered, this,
        &LayoutsHandler::at_removeLayoutItemFromSceneAction_triggered);

    connect(action(action::OpenInNewTabAction), &QAction::triggered, this,
        &LayoutsHandler::at_openInNewTabAction_triggered);

    connect(action(action::OpenIntercomLayoutAction), &QAction::triggered, this,
        &LayoutsHandler::at_openIntercomLayoutAction_triggered);

    connect(appContext()->unifiedResourcePool(), &UnifiedResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            auto layouts = resources.filtered<LayoutResource>();
            if (!layouts.empty())
                workbench()->removeLayouts(layouts);

            if (qnClientMessageProcessor->connectionStatus()->state() == QnConnectionState::Ready
                && workbench()->layouts().empty()
                && !workbench()->isInSessionRestoreProcess())
            {
                menu()->trigger(action::OpenNewTabAction);
            }
        });

    connect(accessController(), &QnWorkbenchAccessController::permissionsChanged, this,
        [this](const QnResourcePtr& resource)
        {
            // Remove layouts if current user has lost access to them.
            if (auto layoutResource = resource.dynamicCast<LayoutResource>();
                layoutResource
                && !accessController()->hasPermissions(layoutResource, Qn::ReadPermission))
            {
                workbench()->removeLayout(layoutResource);
            }
        });

    connect(resourceAccessProvider(), &nx::core::access::ResourceAccessProvider::accessChanged, this,
        [this](const QnResourceAccessSubject& subject, const QnResourcePtr& resource)
        {
            if (!subject.isUser())
                return;

            const auto currentUser = context()->user();
            if (!currentUser)
                return;

            if (currentUser->getId() != subject.id())
                return;

            const auto resourceFlags = resource->flags();
            if (!resourceFlags.testFlag(Qn::layout) || !resourceFlags.testFlag(Qn::remote))
                return;

            if (resourceFlags.testFlag(Qn::removed))
                return;

            if (qnClientMessageProcessor->connectionStatus()->state() == QnConnectionState::Ready
                && workbench()->layouts().empty())
            {
                menu()->trigger(action::OpenNewTabAction);
            }
        });

    connect(systemContext()->messageProcessor(), &QnCommonMessageProcessor::businessActionReceived,
        this, &LayoutsHandler::at_openLayoutAction_triggered);
}

LayoutsHandler::~LayoutsHandler()
{
}

void LayoutsHandler::at_openLayoutAction_triggered(
    const vms::event::AbstractActionPtr& businessAction)
{
    if (businessAction->actionType() != vms::api::ActionType::openLayoutAction)
        return;

    const auto& actionParams = businessAction->getParams();
    const auto layout = resourcePool()->getResourceById<LayoutResource>(
        actionParams.actionResourceId);
    if (!layout)
        return;

    // We can get here while connecting if there are a lot of resources on the server.
    const auto currentUser = context()->user();
    if (!currentUser)
        return;

    if (!actionParams.allUsers)
    {
        const auto checkUserAcessibility =
            [&]()
            {
                // Either user or his role should be mentioned in actionParams.additionalResources
                // so that user can handle this action.
                const auto& permitted = actionParams.additionalResources;
                if (std::find(permitted.begin(), permitted.end(), currentUser->getId()) != permitted.end())
                    return true;

                for (const auto& roleId: currentUser->allUserRoleIds())
                {
                    if (std::find(permitted.begin(), permitted.end(), roleId) != permitted.end())
                        return true;
                }
                return false;
            };

        if (!checkUserAcessibility())
            return;
    }

    if (ResourceAccessManager::hasPermissions(layout, Qn::ReadPermission))
    {
        using namespace std::chrono;
        menu()->trigger(action::OpenInNewTabAction, layout);
        if (actionParams.recordBeforeMs > 0)
        {
            const microseconds navigationTime = qnSyncTime->currentTimePoint()
                - milliseconds(actionParams.recordBeforeMs);

            using namespace ui::action;
            menu()->triggerIfPossible(JumpToTimeAction,
                Parameters().withArgument(Qn::TimestampRole, navigationTime));
        }
    }
}

void LayoutsHandler::at_forgetLayoutPasswordAction_triggered()
{
    using namespace nx::vms::client::desktop;

    auto layout = menu()->currentParameters(sender()).resource().dynamicCast<QnFileLayoutResource>();
    NX_ASSERT(layout && layout::isEncrypted(layout));

    if (!layout || !layout->isEncrypted() || layout->requiresPassword())
        return;

    // This should be done before layout updates, because QnWorkbenchLayout overwrites
    // all data in the layout.
    workbench()->removeLayout(layout);

    layout::reloadFromFile(layout); //< Actually reload the file without a password.

    // Clear "modified" flag from layout.
    auto systemContext = SystemContext::fromResource(layout);
    if (NX_ASSERT(systemContext))
        systemContext->layoutSnapshotManager()->store(layout);
}

void LayoutsHandler::renameLayout(const LayoutResourcePtr &layout, const QString &newName)
{
    auto systemContext = SystemContext::fromResource(layout);
    if (!NX_ASSERT(systemContext))
        return;

    LayoutResourceList existing = alreadyExistingLayouts(
        systemContext->resourcePool(),
        newName,
        layout->getParentId(),
        layout);

    if (!canRemoveLayouts(existing))
    {
        ui::messages::Resources::layoutAlreadyExists(mainWindowWidget());
        return;
    }

    if (!existing.isEmpty())
    {
        if (!ui::messages::Resources::overrideLayout(mainWindowWidget()))
            return;

        removeLayouts(existing);
    }

    bool changed = systemContext->layoutSnapshotManager()->isChanged(layout);

    layout->setName(newName);

    if (!changed)
        systemContext->layoutSnapshotManager()->save(layout);
}

void LayoutsHandler::saveLayout(const LayoutResourcePtr& layout)
{
    if (!layout)
        return;

    // This scenario is not very actual as it can be caused only if user places cross-system
    // cameras on a local layout using api.
    if (!isCloudLayout(layout) && hasCrossSystemItems(layout))
    {
        appContext()->cloudLayoutsManager()->convertLocalLayout(layout);
        return;
    }

    auto systemContext = SystemContext::fromResource(layout);
    if (!NX_ASSERT(systemContext))
        return;

    auto snapshotManager = systemContext->layoutSnapshotManager();
    if (!snapshotManager->isSaveable(layout))
        return;

    if (!ResourceAccessManager::hasPermissions(layout, Qn::SavePermission))
        return;

    if (layout->isFile())
    {
        menu()->trigger(action::SaveLocalLayoutAction, layout);
    }
    else if (isCloudLayout(layout))
    {
        snapshotManager->save(layout);
        return;
    }
    else if (layout->isVideoWallReviewLayout())
    {
        // TODO: #sivanov Refactor common code to common place.
        NX_ASSERT(ResourceAccessManager::hasPermissions(layout, Qn::SavePermission),
            "Saving unsaveable resource");
        if (context()->instance<QnWorkbenchVideoWallHandler>()->saveReviewLayout(layout,
                nx::utils::guarded(this,
                    [this, layout, snapshotManager](int /*reqId*/, ec2::ErrorCode errorCode)
                {
                    snapshotManager->markBeingSaved(layout->getId(), false);
                    if (errorCode != ec2::ErrorCode::ok)
                        return;
                    snapshotManager->markChanged(layout->getId(), false);
                })))
        {
            snapshotManager->markBeingSaved(layout->getId(), true);
        }
    }
    else
    {
        // TODO: #sivanov Check existing layouts. All remotes layout checking and saving should be
        // done in one place.

        const auto change = calculateLayoutChange(layout);
        const auto layoutOwner = layout->getParentResource();

        if (confirmLayoutChange(change, layoutOwner))
        {
            const auto user = layoutOwner.dynamicCast<QnUserResource>();
            if (user)
                grantMissingAccessRights(user, change);

            snapshotManager->save(layout);
        }
        else
        {
            snapshotManager->restore(layout);
        }
    }
}

void LayoutsHandler::saveLayoutAs(const LayoutResourcePtr& layout)
{
    if (!NX_ASSERT(layout))
        return;

    if (isCloudLayout(layout))
        saveCloudLayoutAs(layout);
    else
        saveRemoteLayoutAs(layout);
}

void LayoutsHandler::saveRemoteLayoutAs(const LayoutResourcePtr& layout)
{
    QnUserResourcePtr user = context()->user();
    if (!NX_ASSERT(layout) || !NX_ASSERT(user))
        return;

    if (!layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull())
        return;

    const bool isOwnLayout = (user->getId() == layout->getParentId());
    bool hasSavePermission = ResourceAccessManager::hasPermissions(layout, Qn::SavePermission);

    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, mainWindowWidget()));
    dialog->setWindowTitle(tr("Save Layout As"));
    dialog->setText(tr("Enter Layout Name:"));

    QString proposedName = hasSavePermission
        ? layout->getName()
        : generateUniqueLayoutName(
            resourcePool(), context()->user(), layout->getName(), layout->getName() + lit(" %1"));

    dialog->setName(proposedName);
    setHelpTopic(dialog.data(), Qn::SaveLayout_Help);

    QString name;
    do
    {
        if (!dialog->exec())
            return;

        if (dialog->clickedButton() != QDialogButtonBox::Save)
            return;

        name = dialog->name();

        // that's the case when user press "Save As" and enters the same name as this layout
        // already has
        if (name == layout->getName() && isOwnLayout && hasSavePermission)
        {
            if (layout->hasFlags(Qn::local))
            {
                saveLayout(layout);
                return;
            }

            if (!ui::messages::Resources::overrideLayout(mainWindowWidget()))
                return;

            saveLayout(layout);
            return;
        }

        /* Check if we have rights to overwrite the layout */
        LayoutResourcePtr excludingSelfLayout =
            hasSavePermission ? layout : LayoutResourcePtr();
        LayoutResourceList existing =
            alreadyExistingLayouts(resourcePool(), name, user->getId(), excludingSelfLayout);
        if (!canRemoveLayouts(existing))
        {
            ui::messages::Resources::layoutAlreadyExists(mainWindowWidget());
            dialog->setName(proposedName);
            continue;
        }

        if (!existing.isEmpty())
        {
            if (!ui::messages::Resources::overrideLayout(mainWindowWidget()))
                return;

            removeLayouts(existing);
        }
        break;

    } while (true);

    LayoutResource::ItemsRemapHash newUuidByOldUuid;
    LayoutResourcePtr newLayout = layout->clone(&newUuidByOldUuid);
    newLayout->setName(name);
    newLayout->setParentId(user->getId());
    resourcePool()->addResource(newLayout);

    const bool isCurrent = (layout == workbench()->currentLayout()->resource());

    // We can "Save as" temporary layouts like Alarm layout or Preview Search or Audit Trail.
    const bool isTemporaryLayout = !layout->systemContext();

    const auto systemContext = isTemporaryLayout
        ? appContext()->currentSystemContext()
        : SystemContext::fromResource(layout);

    if (!NX_ASSERT(systemContext))
        return;

    auto snapshotManager = systemContext->layoutSnapshotManager();

    // Can replace only own or temporary layout.
    const bool canReplaceLayout = isOwnLayout || isTemporaryLayout;

    // Delete local layout when replacing it with a remote one.
    const bool shouldDelete = layout->hasFlags(Qn::local)
        && (name == layout->getName() || isCurrent);

    // If it is current layout, close it and open the new one instead.
    if (isCurrent && canReplaceLayout)
    {
        workbench()->replaceLayout(layout, newLayout);

        // If current layout should not be deleted then roll it back
        if (!shouldDelete && !isTemporaryLayout)
            snapshotManager->restore(layout);
    }

    snapshotManager->save(newLayout);

    const auto radassManager = context()->instance<RadassResourceManager>();
    for (auto it = newUuidByOldUuid.begin(); it != newUuidByOldUuid.end(); ++it)
    {
        const auto mode = radassManager->mode(LayoutItemIndex(layout, it.key()));
        const auto newItemIndex = LayoutItemIndex(newLayout, it.value());
        radassManager->setMode(newItemIndex, mode);
    }

    if (!globalSettings()->localSystemId().isNull())
        radassManager->saveData(globalSettings()->localSystemId(), resourcePool());

    if (shouldDelete)
        removeLayouts(LayoutResourceList() << layout);
}

void LayoutsHandler::saveLayoutAsCloud(const LayoutResourcePtr& layout)
{
    if (!NX_ASSERT(layout))
        return;

    NX_ASSERT(!isCloudLayout(layout));

    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel,
        mainWindowWidget()));
    dialog->setWindowTitle(tr("Save Layout As Cloud"));
    dialog->setText(tr("Enter Layout Name:"));

    const QString proposedName = layout->getName();
    dialog->setName(proposedName);

    if (!dialog->exec() || dialog->clickedButton() != QDialogButtonBox::Save)
        return;

    const QString name = dialog->name();

    auto cloudResourcesPool = appContext()->cloudLayoutsSystemContext()->resourcePool();

    LayoutResourceList existing = cloudResourcesPool->getResources<LayoutResource>(
        [suggestedName = name.toLower()](const LayoutResourcePtr& layout)
        {
            return suggestedName == layout->getName().toLower();
        });

    if (!existing.isEmpty())
    {
        if (!ui::messages::Resources::overrideLayout(mainWindowWidget()))
            return;

        removeLayouts(existing);
    }

    // Convert common layout to cloud one.
    auto cloudLayout = appContext()->cloudLayoutsManager()->convertLocalLayout(layout);
    cloudLayout->setName(name);
    saveLayout(cloudLayout);
    workbench()->replaceLayout(layout, cloudLayout);
}

void LayoutsHandler::saveCloudLayoutAs(const LayoutResourcePtr& layout)
{
    if (!NX_ASSERT(isCloudLayout(layout)))
        return;

    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel,
        mainWindowWidget()));
    dialog->setWindowTitle(tr("Save Layout As"));
    dialog->setText(tr("Enter Layout Name:"));

    const QString proposedName = layout->getName();
    dialog->setName(proposedName);

    if (!dialog->exec() || dialog->clickedButton() != QDialogButtonBox::Save)
        return;

    const QString name = dialog->name();

    // User press "Save As" and enters the same name as this layout already has.
    if (name == proposedName)
    {
        saveLayout(layout);
        return;
    }

    auto cloudResourcesPool = appContext()->cloudLayoutsSystemContext()->resourcePool();

    LayoutResourceList existing = cloudResourcesPool->getResources<LayoutResource>(
        [suggestedName = name.toLower()](const LayoutResourcePtr& layout)
        {
            return suggestedName == layout->getName().toLower();
        });

    if (!existing.isEmpty())
    {
        if (!ui::messages::Resources::overrideLayout(mainWindowWidget()))
            return;

        removeLayouts(existing);
    }

    auto cloudLayout = layout->clone();
    NX_ASSERT(isCloudLayout(cloudLayout));
    cloudLayout->addFlags(Qn::local);
    cloudLayout->setName(name);
    appContext()->cloudLayoutsSystemContext()->resourcePool()->addResource(cloudLayout);
    saveLayout(cloudLayout);
    workbench()->replaceLayout(layout, cloudLayout);
}

void LayoutsHandler::removeLayoutItems(const LayoutItemIndexList& items, bool autoSave)
{
    if (items.size() > 1)
    {
        const auto layout = items.first().layout();
        const bool isShowreel = layout->isShowreelReviewLayout();
        const auto resources = action::ParameterTypes::resources(items);

        const bool confirm = isShowreel
            ? ui::messages::Resources::removeItemsFromLayoutTour(mainWindowWidget(), resources)
            : ui::messages::Resources::removeItemsFromLayout(mainWindowWidget(), resources);

        if (!confirm)
            return;
    }

    QList<QnUuid> orphanedUuids;
    QSet<LayoutResourcePtr> layouts;
    for (const LayoutItemIndex &index : items)
    {
        if (index.layout())
        {
            index.layout()->removeItem(index.uuid());
            layouts << index.layout();
        }
        else
        {
            orphanedUuids.push_back(index.uuid());
        }
    }

    /* If appserver is not running, we may get removal requests without layout resource. */
    if (!orphanedUuids.isEmpty())
    {
        QList<QnWorkbenchLayout *> layouts;
        layouts.push_front(workbench()->currentLayout());
        for (const QnUuid &uuid : orphanedUuids)
        {
            for (QnWorkbenchLayout* layout : layouts)
            {
                if (QnWorkbenchItem* item = layout->item(uuid))
                {
                    qnDeleteLater(item);
                    break;
                }
            }
        }
    }

    if (workbench()->currentLayout()->items().isEmpty())
        workbench()->currentLayoutResource()->setCellAspectRatio(-1.0);

    if (autoSave)
    {
        for (const auto& layout : layouts)
            menu()->trigger(action::SaveLayoutAction, layout);
    }
}

LayoutsHandler::LayoutChange LayoutsHandler::calculateLayoutChange(
    const LayoutResourcePtr& layout)
{
    auto systemContext = SystemContext::fromResource(layout);
    if (!NX_ASSERT(systemContext))
        return {};

    LayoutChange result;
    result.layout = layout;

    /* Share added resources. */
    const auto& snapshot = systemContext->layoutSnapshotManager()->snapshot(layout);

    // Check only Resources from the same System Context as the Layout. Cross-system Resources
    // cannot be available through Shared Layouts.
    auto oldResources = localLayoutResources(systemContext->resourcePool(), snapshot.items);
    auto newResources = localLayoutResources(layout);

    result.added = (newResources - oldResources).values();
    result.removed = (oldResources - newResources).values();

    return result;
}

bool LayoutsHandler::confirmLayoutChange(
    const LayoutChange& change,
    const QnResourcePtr& layoutOwner)
{
    NX_ASSERT(context()->user(), "Should never ask for layout saving when offline");
    if (!context()->user())
        return false;

    /* Never ask for own layouts. */
    if (layoutOwner == context()->user())
        return true;

    /* Ask confirmation if user will lose some of cameras due to videowall layout change. */
    if (layoutOwner && layoutOwner->hasFlags(Qn::videowall))
        return confirmChangeVideoWallLayout(change);

    /* Never ask for auto-generated (e.g. preview search) layouts. */
    if (change.layout->isServiceLayout())
        return true;

    /* Check shared layout */
    if (change.layout->isShared())
        return confirmChangeSharedLayout(change);

    /* Never ask for intercom layouts. */
    if (change.layout->isIntercomLayout())
        return true;

    return confirmChangeLocalLayout(layoutOwner.dynamicCast<QnUserResource>(), change);
}

bool LayoutsHandler::confirmChangeSharedLayout(const LayoutChange& change)
{
    /* Checking if custom users have access to this shared layout. */
    auto allUsers = resourcePool()->getResources<QnUserResource>();
    auto accessibleToCustomUsers = std::any_of(
        allUsers.cbegin(), allUsers.cend(),
        [this, layout = change.layout](const QnUserResourcePtr& user)
        {
            return resourceAccessProvider()->accessibleVia(user, layout) ==
                nx::core::access::Source::shared;
        });

    /* Do not warn if there are no such users - no side effects in any case. */
    if (!accessibleToCustomUsers)
        return true;

    return ui::messages::Resources::sharedLayoutEdit(mainWindowWidget());
}

bool LayoutsHandler::confirmDeleteSharedLayouts(const LayoutResourceList& layouts)
{
    /* Checking if custom users have access to this shared layout. */
    auto allUsers = resourcePool()->getResources<QnUserResource>();
    auto accessibleToCustomUsers = std::any_of(allUsers.cbegin(), allUsers.cend(),
        [this, layouts](const QnUserResourcePtr& user)
        {
            return std::any_of(layouts.cbegin(), layouts.cend(),
                [this, user](const LayoutResourcePtr& layout)
                {
                    return resourceAccessProvider()->accessibleVia(user, layout) ==
                        nx::core::access::Source::shared;
                });
        });

    /* Do not warn if there are no such users - no side effects in any case. */
    if (!accessibleToCustomUsers)
        return true;

    return ui::messages::Resources::deleteSharedLayouts(mainWindowWidget(), layouts);
}

bool LayoutsHandler::confirmChangeLocalLayout(const QnUserResourcePtr& user,
    const LayoutChange& change)
{
    NX_ASSERT(user);
    if (!user)
        return true;

    /* Calculate removed cameras that are still directly accessible. */
    switch (user->userRole())
    {
        case Qn::UserRole::customPermissions:
            return ui::messages::Resources::changeUserLocalLayout(mainWindowWidget(), change.removed);
        case Qn::UserRole::customUserRole:
            return ui::messages::Resources::addToRoleLocalLayout(
                    mainWindowWidget(),
                    calculateResourcesToShare(change.added, user))
                && ui::messages::Resources::removeFromRoleLocalLayout(
                    mainWindowWidget(),
                    change.removed);
        default:
            break;
    }
    return true;
}

bool LayoutsHandler::confirmDeleteLocalLayouts(const QnUserResourcePtr& user,
    const LayoutResourceList& layouts)
{
    NX_ASSERT(user);
    if (!user)
        return true;

    if (resourceAccessManager()->hasGlobalPermission(user, GlobalPermission::accessAllMedia))
        return true;

    /* Never ask for own layouts. */
    if (user == context()->user())
        return true;

    // Calculate removed cameras that are still directly accessible.
    // Check only Resources from the same System Context as the Layout. Cross-system Resources
    // cannot be available through Shared Layouts.
    QSet<QnResourcePtr> removedResources;
    for (const auto& layout: layouts)
    {
        NX_ASSERT(!isCloudLayout(layout));

        auto systemContext = SystemContext::fromResource(layout);
        if (!NX_ASSERT(systemContext))
            continue;

        const auto& snapshot = systemContext->layoutSnapshotManager()->snapshot(layout);
        removedResources.unite(localLayoutResources(resourcePool(), snapshot.items));
    }

    const auto accessible = sharedResourcesManager()->sharedResources(user);
    QnResourceList stillAccessible;
    for (const QnResourcePtr& resource: std::as_const(removedResources))
    {
        QnUuid id = resource->getId();
        if (accessible.contains(id))
            stillAccessible.append(resource);
    }

    return ui::messages::Resources::deleteLocalLayouts(mainWindowWidget(), stillAccessible);
}

bool LayoutsHandler::confirmChangeVideoWallLayout(const LayoutChange& change)
{
    QnWorkbenchLayoutsChangeValidator validator(context());
    return validator.confirmChangeVideoWallLayout(change.layout, change.removed);
}

void LayoutsHandler::grantMissingAccessRights(const QnUserResourcePtr& user,
    const LayoutChange& change)
{
    NX_ASSERT(user);
    if (!user)
        return;

    if (resourceAccessManager()->hasGlobalPermission(user, GlobalPermission::accessAllMedia))
        return;

    QnResourceAccessSubject subject(user);
    // This is required to keep old behavior when it was not possible to grant access to the user
    // directly if he has a role.
    if (const auto roleIds = user->userRoleIds(); !roleIds.empty())
        subject = nx::vms::api::UserRoleData(roleIds.front(), "");

    auto accessible = sharedResourcesManager()->sharedResources(subject);
    for (const auto& toShare : calculateResourcesToShare(change.added, user))
        accessible << toShare->getId();
    qnResourcesChangesManager->saveAccessibleResources(subject, accessible);
}

bool LayoutsHandler::canRemoveLayouts(const LayoutResourceList &layouts)
{
    return std::all_of(layouts.cbegin(), layouts.cend(),
        [this](const LayoutResourcePtr& layout)
        {
            return ResourceAccessManager::hasPermissions(layout, Qn::RemovePermission);
        });
}

void LayoutsHandler::removeLayouts(const LayoutResourceList &layouts)
{
    if (layouts.isEmpty())
        return;

    if (!canRemoveLayouts(layouts))
        return;

    LayoutResourceList remoteResources;
    for (const LayoutResourcePtr& layout: layouts)
    {
        NX_ASSERT(!layout->isFile());
        if (layout->isFile())
            continue;

        if (isCloudLayout(layout))
            appContext()->cloudLayoutsManager()->deleteLayout(layout);
        else if (layout->hasFlags(Qn::local))
            resourcePool()->removeResource(layout); /*< This one can be simply deleted from resource pool. */
        else
            remoteResources << layout;
    }

    qnResourcesChangesManager->deleteResources(remoteResources);
}

bool LayoutsHandler::closeLayouts(const QnWorkbenchLayoutList& layouts)
{
    LayoutResourceList resources;
    for (auto layout: layouts)
        resources.push_back(layout->resource());

    return closeLayouts(resources);
}

bool LayoutsHandler::closeLayouts(const LayoutResourceList& resources)
{
    if (resources.empty())
        return true;

    workbench()->removeLayouts(resources);
    if (workbench()->layouts().empty())
        menu()->trigger(action::OpenNewTabAction);

    return true;
}

void LayoutsHandler::openLayouts(
    const LayoutResourceList& layouts,
    const StreamSynchronizationState& playbackState)
{
    if (!NX_ASSERT(!layouts.empty()))
        return;

    QnWorkbenchLayout* lastLayout = nullptr;
    for (const auto& layout: layouts)
    {
        auto wbLayout = workbench()->layout(layout);
        if (!wbLayout)
        {
            wbLayout = workbench()->addLayout(layout);

            if (!wbLayout->isPreviewSearchLayout())
            {
                // Use zero position for nov files.
                const auto state = layout->isFile()
                    ? StreamSynchronizationState::playFromStart()
                    : playbackState;

                // Force playback state.
                wbLayout->setStreamSynchronizationState(state);

                for (auto item: wbLayout->items())
                {
                    // Do not set item's playback parameters if they are already set.
                    if (!item->data(Qn::ItemTimeRole).isValid())
                    {
                        item->setData(Qn::ItemTimeRole,
                            state.timeUs == DATETIME_NOW ? DATETIME_NOW : state.timeUs / 1000);
                        item->setData(Qn::ItemPausedRole, state.speed == 0);
                        item->setData(Qn::ItemSpeedRole, state.speed);
                    }
                }
            }
        }
        // Explicitly set that we do not control videowall through this layout.
        wbLayout->setData(Qn::VideoWallItemGuidRole, QVariant::fromValue(QnUuid()));

        lastLayout = wbLayout;
    }
    workbench()->setCurrentLayout(lastLayout);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void LayoutsHandler::at_newUserLayoutAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    auto user = parameters.hasArgument(Qn::UserResourceRole)
        ? parameters.argument(Qn::UserResourceRole).value<QnUserResourcePtr>()
        : parameters.resource().dynamicCast<QnUserResource>();

    if (!user)
        user = context()->user();

    if (!user)
        return;

    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, mainWindowWidget()));
    dialog->setWindowTitle(tr("New Layout"));
    dialog->setText(tr("Enter the name of the layout to create:"));
    dialog->setName(generateUniqueLayoutName(resourcePool(), user, tr("New Layout"), tr("New Layout %1")));
    dialog->setWindowModality(Qt::ApplicationModal);

    if (!dialog->exec())
        return;

    LayoutResourceList existing = alreadyExistingLayouts(resourcePool(), dialog->name(), user->getId());
    if (!canRemoveLayouts(existing))
    {
        ui::messages::Resources::layoutAlreadyExists(mainWindowWidget());
        return;
    }

    if (!existing.isEmpty())
    {
        bool allAreLocal = std::all_of(existing.cbegin(), existing.cend(),
            [](const LayoutResourcePtr& layout)
            {
                return layout->hasFlags(Qn::local);
            });

        if (!allAreLocal && !ui::messages::Resources::overrideLayout(mainWindowWidget()))
            return;

        removeLayouts(existing);
    }

    LayoutResourcePtr layout(new LayoutResource());
    layout->setIdUnsafe(QnUuid::createUuid());
    layout->setName(dialog->name());
    layout->setParentId(user->getId());
    resourcePool()->addResource(layout);

    appContext()->currentSystemContext()->layoutSnapshotManager()->save(layout);

    menu()->trigger(action::OpenInNewTabAction, layout);
}

void LayoutsHandler::at_closeLayoutAction_triggered()
{
    auto layouts = menu()->currentParameters(sender()).layouts();
    if (layouts.empty() && workbench()->layouts().size() > 1)
        layouts.push_back(workbench()->currentLayout());
    closeLayouts(layouts);
}

void LayoutsHandler::at_closeAllButThisLayoutAction_triggered()
{
    QnWorkbenchLayoutList layouts = menu()->currentParameters(sender()).layouts();
    if (layouts.size() != 1)
        return;

    auto selectedLayout = layouts[0];
    workbench()->setCurrentLayout(selectedLayout);

    LayoutResourceList layoutsToClose;
    for (const auto& layout: workbench()->layouts())
        layoutsToClose.push_back(layout->resource());

    layoutsToClose.removeOne(selectedLayout->resource());
    closeLayouts(layoutsToClose);
}

void LayoutsHandler::at_removeFromServerAction_triggered()
{
    auto layouts = menu()->currentParameters(sender()).resources().filtered<LayoutResource>();

    LayoutResourceList canAutoDelete;
    LayoutResourceList shared;
    QHash<QnUserResourcePtr, LayoutResourceList> common;
    for (const auto& layout: layouts)
    {
        NX_ASSERT(!layout->isFile());
        if (layout->isFile())
            continue;

        auto owner = layout->getParentResource().dynamicCast<QnUserResource>();
        if (isCloudLayout(layout) || layout->isServiceLayout() || layout->hasFlags(Qn::local))
            canAutoDelete << layout;
        else if (layout->isShared())
            shared << layout;
        else if (owner)
            common[owner] << layout;
        else
            canAutoDelete << layout; /* Invalid layout */
    }
    removeLayouts(canAutoDelete);

    if (confirmDeleteSharedLayouts(shared))
        removeLayouts(shared);

    auto users = common.keys();
    std::sort(
        users.begin(), users.end(),
        [](const QnUserResourcePtr& l, const QnUserResourcePtr &r)
        {
            return nx::utils::naturalStringLess(l->getName(), r->getName());
        });

    for (const auto &user : users)
    {
        auto userLayouts = common[user];
        NX_ASSERT(!userLayouts.isEmpty());
        if (confirmDeleteLocalLayouts(user, userLayouts))
            removeLayouts(userLayouts);
    }
}

void LayoutsHandler::at_openNewTabAction_triggered()
{
    auto resource = LayoutResourcePtr(new LayoutResource());
    resource->setIdUnsafe(QnUuid::createUuid());
    resource->addFlags(Qn::local);
    resource->setName(generateUniqueLayoutName(
        resourcePool(),
        context()->user(),
        tr("New Layout"),
        tr("New Layout %1")));
    if (context()->user())
        resource->setParentId(context()->user()->getId());

    resourcePool()->addResource(resource);

    const auto parameters = menu()->currentParameters(sender());
    QnWorkbenchLayout* layout = workbench()->addLayout(resource);
    if (parameters.hasArgument(Qn::LayoutSyncStateRole))
    {
        const auto syncState = parameters.argument(Qn::LayoutSyncStateRole);
        layout->setStreamSynchronizationState(
            parameters.argument<StreamSynchronizationState>(Qn::LayoutSyncStateRole));
    }
    else
    {
        layout->setStreamSynchronizationState(StreamSynchronizationState::live());
    }

    workbench()->setCurrentLayout(layout);
}

void LayoutsHandler::at_removeLayoutItemAction_triggered()
{
    removeLayoutItems(menu()->currentParameters(sender()).layoutItems(), true);
}

void LayoutsHandler::at_removeLayoutItemFromSceneAction_triggered()
{
    const auto layoutItems = menu()->currentParameters(sender()).layoutItems();
    removeLayoutItems(layoutItems, false);
}

void LayoutsHandler::at_openInNewTabAction_triggered()
{
    const auto isCameraWithFootage =
        [this](const QnMediaResourceWidget* widget)
        {
            const auto camera = widget->resource().dynamicCast<QnVirtualCameraResource>();
            return camera
                && ResourceAccessManager::hasPermissions(camera, Qn::ViewFootagePermission);
        };

    auto parameters = menu()->currentParameters(sender());
    const bool calledFromScene = parameters.widgets().size() > 0;
    const QnMediaResourceWidget* currentMediaWidget = calledFromScene
        ? qobject_cast<QnMediaResourceWidget*>(parameters.widgets().first())
        : navigator()->currentMediaWidget();

    QnLayoutItemDataList items;
    if (calledFromScene)
    {
        for (const auto& widget: parameters.widgets())
            items.push_back(widget->item()->data());
        items = LayoutResource::cloneItems(items);
    }

    const auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();

    // Update state depending on how the action was called.
    auto currentState = streamSynchronizer->state();
    if (!currentMediaWidget || !isCameraWithFootage(currentMediaWidget))
    {
        // Switch to live if current item is not a camera or has no footage.
        currentState = StreamSynchronizationState::live();
    }
    else if (calledFromScene)
    {
        currentState.isSyncOn = true; //< Force sync if several cameras were selected.
        currentState.timeUs = std::chrono::microseconds(currentMediaWidget->position()).count();
    }
    else if (!currentState.isSyncOn)
    {
        // Open resources from tree in default state if current layouts is not synced.
        currentState = StreamSynchronizationState::live();
    }

    // Stop showreel if it is running. Here widgets can be destroyed, so we should remove them
    // from parameters to avoid AV.
    const auto resources = parameters.resources();
    parameters.setResources(resources);
    if (action(action::ToggleLayoutTourModeAction)->isChecked())
        menu()->trigger(action::ToggleLayoutTourModeAction);

    const auto layouts = resources.filtered<LayoutResource>();
    if (!layouts.empty())
    {
        NX_ASSERT(!calledFromScene, "Layouts can not be passed from the scene");
        NX_ASSERT(resources.size() == layouts.size(), "Mixed resources are not expected here");
        openLayouts(layouts, currentState);
        // Do not return in case mixed resource set is passed somehow.
    }

    const auto openable = resources.filtered(QnResourceAccessFilter::isOpenableInLayout);

    if (openable.empty())
        return;

    const bool hasCrossSystemResources = std::any_of(
        openable.cbegin(), openable.cend(),
        [](const QnResourcePtr& resource) { return resource->hasFlags(Qn::cross_system); });

    if (hasCrossSystemResources && !CloudLayoutsIntroDialog::confirm())
        return;

    auto layout = hasCrossSystemResources
        ? LayoutResourcePtr(new CrossSystemLayoutResource())
        : LayoutResourcePtr(new LayoutResource());
    layout->setIdUnsafe(QnUuid::createUuid());
    layout->addFlags(Qn::local);

    if (hasCrossSystemResources)
    {
        appContext()->cloudLayoutsSystemContext()->resourcePool()->addResource(layout);
        layout->setName(generateUniqueLayoutName(
            layout->resourcePool(),
            /*user*/ {},
            tr("New Layout"),
            tr("New Layout %1")));
    }
    else
    {
        if (context()->user())
            layout->setParentId(context()->user()->getId());
        resourcePool()->addResource(layout);
        layout->setName(generateUniqueLayoutName(
            resourcePool(),
            context()->user(),
            tr("New Layout"),
            tr("New Layout %1")));
    }

    if (calledFromScene)
    {
        layout->setItems(items);
        openLayouts({layout}, currentState);
    }
    else
    {
        openLayouts({layout}, currentState);
        // Remove layouts from the list of resources (if there were any).
        parameters.setResources(openable);
        parameters.setArgument(Qn::LayoutSyncStateRole, currentState);
        menu()->trigger(action::DropResourcesAction, parameters);
    }
}

void LayoutsHandler::at_openIntercomLayoutAction_triggered()
{
    at_openInNewTabAction_triggered();

    const ui::action::Parameters parameters = menu()->currentParameters(sender());

    const auto businessAction =
        parameters.argument<vms::event::AbstractActionPtr>(Qn::ActionDataRole);

    const auto broadcastAction = nx::vms::event::CommonAction::createBroadcastAction(
        nx::vms::api::ActionType::showIntercomInformer, businessAction->getParams());
    broadcastAction->setToggleState(nx::vms::api::EventState::inactive);

    if (const auto connection = messageBusConnection())
    {
        const auto manager = connection->getEventRulesManager(Qn::kSystemAccess);
        nx::vms::api::EventActionData actionData;
        ec2::fromResourceToApi(broadcastAction, actionData);
        manager->broadcastEventAction(actionData, [](int /*handle*/, ec2::ErrorCode) {});
    }
}

bool LayoutsHandler::tryClose(bool /*force*/)
{
    workbench()->clear();
    return true;
}

void LayoutsHandler::forcedUpdate()
{
    // Do nothing.
}

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
