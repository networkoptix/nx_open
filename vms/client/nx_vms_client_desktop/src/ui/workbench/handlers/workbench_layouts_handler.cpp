// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_layouts_handler.h"

#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

#include <QtWidgets/QAction>

#include <client/client_globals.h>
#include <client/client_message_processor.h>
#include <client/client_settings.h>
#include <common/common_module.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/layout_reader.h>
#include <core/resource/layout_resource.h>
#include <core/resource/resource.h>
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
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/radass/radass_resource_manager.h>
#include <nx/vms/client/desktop/resources/layout_password_management.h>
#include <nx/vms/client/desktop/resources/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resources/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameter_types.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/client/desktop/workbench/layouts/layout_factory.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <ui/dialogs/layout_name_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/extensions/workbench_layout_change_validator.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/handlers/workbench_videowall_handler.h> //< TODO: #sivanov Dependencies.
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_state_manager.h>
#include <utils/common/delete_later.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>
#include <watchers/cloud_status_watcher.h>

using boost::algorithm::any_of;
using boost::algorithm::all_of;

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
    for (const auto layout : resourcePool->getResources<QnLayoutResource>())
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
QnLayoutResourceList alreadyExistingLayouts(
    QnResourcePool* resourcePool,
    const QString &name,
    const QnUuid &parentId,
    const QnLayoutResourcePtr &layout = QnLayoutResourcePtr())
{
    QnLayoutResourceList result;
    for (const auto& existingLayout : resourcePool->getResourcesByParentId(parentId).filtered<QnLayoutResource>())
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

QSet<QnResourcePtr> localLayoutResources(const QnLayoutResourcePtr& layout)
{
    return localLayoutResources(layout->resourcePool(), layout->getItems());
}

bool isCloudLayout(const QnLayoutResourcePtr& layout)
{
    return layout->hasFlags(Qn::cross_system);
}

bool hasCrossSystemItems(const QnLayoutResourcePtr& layout)
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
    connect(action(action::NewUserLayoutAction), &QAction::triggered, this,
        &LayoutsHandler::at_newUserLayoutAction_triggered);
    connect(action(action::SaveLayoutAction), &QAction::triggered, this,
        &LayoutsHandler::at_saveLayoutAction_triggered);
    connect(action(action::SaveLayoutAsAction), &QAction::triggered, this,
        &LayoutsHandler::at_saveLayoutAsAction_triggered);
    connect(action(action::SaveLayoutAsCloudAction), &QAction::triggered, this,
        &LayoutsHandler::at_saveLayoutAsCloudAction_triggered);
    connect(action(action::SaveLayoutForCurrentUserAsAction), &QAction::triggered, this,
        &LayoutsHandler::at_saveLayoutForCurrentUserAsAction_triggered);
    connect(action(action::SaveCurrentLayoutAction), &QAction::triggered, this,
        &LayoutsHandler::at_saveCurrentLayoutAction_triggered);
    connect(action(action::SaveCurrentLayoutAsAction), &QAction::triggered, this,
        &LayoutsHandler::at_saveCurrentLayoutAsAction_triggered);
    connect(action(action::SaveCurrentLayoutAsCloudAction), &QAction::triggered, this,
        &LayoutsHandler::at_saveCurrentLayoutAsCloudAction_triggered);
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

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (!resource->hasFlags(Qn::layout))
                return;

            auto layoutResource = resource.dynamicCast<QnLayoutResource>();
            if (!layoutResource)
                return;

            if (auto layout = QnWorkbenchLayout::instance(layoutResource))
            {
                workbench()->removeLayout(layout);
                layout->deleteLater();
            }

            if (qnClientMessageProcessor->connectionStatus()->state() == QnConnectionState::Ready
                && workbench()->layouts().empty())
            {
                menu()->trigger(action::OpenNewTabAction);
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

    connect(commonModule()->messageProcessor(), &QnCommonMessageProcessor::businessActionReceived,
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
    const auto layout = resourcePool()->getResourceById<QnLayoutResource>(
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

    if (accessController()->hasPermissions(layout, Qn::ReadPermission))
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
    if (auto workbenchLayout = QnWorkbenchLayout::instance(layout.dynamicCast<QnLayoutResource>()))
    {
        workbench()->removeLayout(workbenchLayout);
        delete workbenchLayout;
    }

    layout::reloadFromFile(layout); //< Actually reload the file without a password.

    // Clear "modified" flag from layout.
    auto systemContext = SystemContext::fromResource(layout);
    if (NX_ASSERT(systemContext))
        systemContext->layoutSnapshotManager()->store(layout);
}

void LayoutsHandler::renameLayout(const QnLayoutResourcePtr &layout, const QString &newName)
{
    auto systemContext = SystemContext::fromResource(layout);
    if (!NX_ASSERT(systemContext))
        return;

    QnLayoutResourceList existing = alreadyExistingLayouts(
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

void LayoutsHandler::saveLayout(const QnLayoutResourcePtr& layout)
{
    if (!layout)
        return;

    // This scenario is not very actual as it can be caused only if user places cross-system
    // cameras on a local layout using api.
    if (ini().crossSystemLayouts
        && !isCloudLayout(layout)
        && hasCrossSystemItems(layout))
    {
        saveLayoutAsCloud(layout);
        return;
    }

    auto systemContext = SystemContext::fromResource(layout);
    if (!NX_ASSERT(systemContext))
        return;

    auto snapshotManager = systemContext->layoutSnapshotManager();
    if (!snapshotManager->isSaveable(layout))
        return;

    if (!accessController()->hasPermissions(layout, Qn::SavePermission))
        return;

    if (layout->isFile())
    {
        menu()->trigger(action::SaveLocalLayoutAction, layout);
    }
    else if (ini().crossSystemLayouts && isCloudLayout(layout))
    {
        snapshotManager->save(layout);
        return;
    }
    else if (!layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull())
    {
        // TODO: #sivanov Refactor common code to common place.
        NX_ASSERT(accessController()->hasPermissions(layout, Qn::SavePermission),
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
        const auto layoutParent = layout->getParentResource();

        const auto user = layoutParent.dynamicCast<QnUserResource>();
        if (confirmLayoutChange(change, user))
        {
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

void LayoutsHandler::saveLayoutAs(const QnLayoutResourcePtr &layout, const QnUserResourcePtr &user)
{
    if (!layout || !user)
        return;

    if (!layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull())
        return;

    const QnResourcePtr layoutOwnerUser = layout->getParentResource();
    bool hasSavePermission = accessController()->hasPermissions(layout, Qn::SavePermission);

    QString name = menu()->currentParameters(sender()).argument<QString>(Qn::ResourceNameRole).trimmed();
    if (name.isEmpty())
    {
        QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Save | QDialogButtonBox::Cancel, mainWindowWidget()));
        dialog->setWindowTitle(tr("Save Layout As"));
        dialog->setText(tr("Enter Layout Name:"));

        QString proposedName = hasSavePermission
            ? layout->getName()
            : generateUniqueLayoutName(resourcePool(), context()->user(), layout->getName(), layout->getName() + lit(" %1"));

        dialog->setName(proposedName);
        setHelpTopic(dialog.data(), Qn::SaveLayout_Help);

        do
        {
            if (!dialog->exec())
                return;

            if (dialog->clickedButton() != QDialogButtonBox::Save)
                return;

            name = dialog->name();

            // that's the case when user press "Save As" and enters the same name as this layout already has
            if (name == layout->getName() && user == layoutOwnerUser && hasSavePermission)
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
            QnLayoutResourcePtr excludingSelfLayout = hasSavePermission ? layout : QnLayoutResourcePtr();
            QnLayoutResourceList existing = alreadyExistingLayouts(resourcePool(), name, user->getId(), excludingSelfLayout);
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
    }
    else
    {
        QnLayoutResourceList existing = alreadyExistingLayouts(resourcePool(), name, user->getId(), layout);
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
    }

    QnLayoutResource::ItemsRemapHash newUuidByOldUuid;
    QnLayoutResourcePtr newLayout = layout->clone(&newUuidByOldUuid);
    newLayout->setName(name);
    newLayout->setParentId(user->getId());
    resourcePool()->addResource(newLayout);

    const bool isCurrent = (layout == workbench()->currentLayout()->resource());
    bool shouldDelete = layout->hasFlags(Qn::local) &&
        (name == layout->getName() || isCurrent);

    auto systemContext = SystemContext::fromResource(layout);
    if (!NX_ASSERT(systemContext))
        return;

    auto snapshotManager = systemContext->layoutSnapshotManager();

    /* If it is current layout, close it and open the new one instead. */
    if (isCurrent &&
        user == layoutOwnerUser)   //making current only new layout of current user
    {
        int index = workbench()->currentLayoutIndex();

        workbench()->insertLayout(qnWorkbenchLayoutsFactory->create(newLayout, this), index);
        workbench()->setCurrentLayoutIndex(index);
        workbench()->removeLayout(index + 1);

        // If current layout should not be deleted then roll it back
        if (!shouldDelete)
        {
            //(user == layoutOwnerUser) condition prevents clearing layout of another user (e.g., if copying layout from one user to another)
                //user - is an owner of newLayout. It is not required to be owner of layout
            snapshotManager->restore(layout);
        }
    }

    snapshotManager->save(newLayout);

    const auto radassManager = context()->instance<RadassResourceManager>();
    for (auto it = newUuidByOldUuid.begin(); it != newUuidByOldUuid.end(); ++it)
    {
        const auto mode = radassManager->mode(QnLayoutItemIndex(layout, it.key()));
        const auto newItemIndex = QnLayoutItemIndex(newLayout, it.value());
        radassManager->setMode(newItemIndex, mode);
    }

    if (!globalSettings()->localSystemId().isNull())
        radassManager->saveData(globalSettings()->localSystemId(), resourcePool());

    if (shouldDelete)
        removeLayouts(QnLayoutResourceList() << layout);
}

void LayoutsHandler::saveLayoutAsCloud(const QnLayoutResourcePtr& layout)
{
    if (!NX_ASSERT(layout) || !NX_ASSERT(ini().crossSystemLayouts))
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

    // User press "Save As" and enters the same name as this layout already has.
    if (name == proposedName && isCloudLayout(layout))
    {
        if (!ui::messages::Resources::overrideLayout(mainWindowWidget()))
            return;

        saveLayout(layout);
        return;
    }

    auto cloudResourcesPool = appContext()->cloudLayoutsSystemContext()->resourcePool();

    QnLayoutResourceList existing = cloudResourcesPool->getResources<QnLayoutResource>(
        [suggestedName = name.toLower()](const QnLayoutResourcePtr& layout)
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
    saveLayout(cloudLayout);

    // Replace opened layout with the cloud one.
    int index = workbench()->currentLayoutIndex();

    workbench()->insertLayout(qnWorkbenchLayoutsFactory->create(cloudLayout, this), index);
    workbench()->setCurrentLayoutIndex(index);
    workbench()->removeLayout(index + 1);
}

void LayoutsHandler::removeLayoutItems(const QnLayoutItemIndexList& items, bool autoSave)
{
    if (items.size() > 1)
    {
        const auto layout = items.first().layout();
        const bool isLayoutTour = !layout->data(Qn::LayoutTourUuidRole).value<QnUuid>().isNull();
        const auto resources = action::ParameterTypes::resources(items);

        const bool confirm = isLayoutTour
            ? ui::messages::Resources::removeItemsFromLayoutTour(mainWindowWidget(), resources)
            : ui::messages::Resources::removeItemsFromLayout(mainWindowWidget(), resources);

        if (!confirm)
            return;
    }

    QList<QnUuid> orphanedUuids;
    QSet<QnLayoutResourcePtr> layouts;
    for (const QnLayoutItemIndex &index : items)
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
        workbench()->currentLayout()->setCellAspectRatio(-1.0);

    if (autoSave)
    {
        for (const auto& layout : layouts)
            menu()->trigger(action::SaveLayoutAction, layout);
    }
}

LayoutsHandler::LayoutChange LayoutsHandler::calculateLayoutChange(
    const QnLayoutResourcePtr& layout)
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
    const QnUserResourcePtr& layoutOwner)
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
    if (nx::vms::common::isIntercomLayout(change.layout))
        return true;

    return confirmChangeLocalLayout(layoutOwner, change);
}

bool LayoutsHandler::confirmChangeSharedLayout(const LayoutChange& change)
{
    /* Checking if custom users have access to this shared layout. */
    auto allUsers = resourcePool()->getResources<QnUserResource>();
    auto accessibleToCustomUsers = any_of(
        allUsers,
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

bool LayoutsHandler::confirmDeleteSharedLayouts(const QnLayoutResourceList& layouts)
{
    /* Checking if custom users have access to this shared layout. */
    auto allUsers = resourcePool()->getResources<QnUserResource>();
    auto accessibleToCustomUsers = any_of(allUsers,
        [this, layouts](const QnUserResourcePtr& user)
        {
            return any_of(layouts,
                [this, user](const QnLayoutResourcePtr& layout)
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
    const QnLayoutResourceList& layouts)
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

bool LayoutsHandler::canRemoveLayouts(const QnLayoutResourceList &layouts)
{
    return all_of(layouts,
        [this](const QnLayoutResourcePtr& layout)
        {
            return accessController()->hasPermissions(layout, Qn::RemovePermission);
        });
}

void LayoutsHandler::removeLayouts(const QnLayoutResourceList &layouts)
{
    if (layouts.isEmpty())
        return;

    if (!canRemoveLayouts(layouts))
        return;

    QnLayoutResourceList remoteResources;
    for (const QnLayoutResourcePtr& layout: layouts)
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

bool LayoutsHandler::closeLayouts(const QnWorkbenchLayoutList &layouts, bool force)
{
    QnLayoutResourceList resources;
    for (auto layout: layouts)
        resources.push_back(layout->resource());

    return closeLayouts(resources, force);
}

bool LayoutsHandler::closeLayouts(
    const QnLayoutResourceList& resources,
    bool force)
{
    if (resources.empty())
        return true;

    QnLayoutResourceList saveableResources, rollbackResources;
    if (!force)
    {
        for (const QnLayoutResourcePtr& layout: resources)
        {
            auto systemContext = SystemContext::fromResource(layout);
            if (!NX_ASSERT(systemContext))
                continue;

            bool changed = systemContext->layoutSnapshotManager()->isChanged(layout);
            if (!changed)
                continue;

            rollbackResources.push_back(layout);
        }
    }

    rollbackResources.append(saveableResources);
    saveableResources.clear();
    closeLayoutsInternal(resources, rollbackResources);
    return true;
}

void LayoutsHandler::closeLayoutsInternal(
    const QnLayoutResourceList& resources,
    const QnLayoutResourceList& rollbackResources)
{
    for (const QnLayoutResourcePtr& layout: rollbackResources)
    {
        auto systemContext = SystemContext::fromResource(layout);
        if (!NX_ASSERT(systemContext))
            continue;

        systemContext->layoutSnapshotManager()->restore(layout);
    }

    for (const QnLayoutResourcePtr& resource: resources)
    {
        if (QnWorkbenchLayout *layout = QnWorkbenchLayout::instance(resource))
        {
            workbench()->removeLayout(layout);
            delete layout;
        }

        auto systemContext = SystemContext::fromResource(resource);
        if (!NX_ASSERT(systemContext))
            continue;

        if (resource->hasFlags(Qn::local)
            && !resource->isFile()
            && !resource->hasFlags(Qn::local_intercom_layout))
        {
            systemContext->resourcePool()->removeResource(resource);
        }
    }

    if (workbench()->layouts().empty())
        menu()->trigger(action::OpenNewTabAction);
}

bool LayoutsHandler::closeAllLayouts(bool force)
{
    return closeLayouts(resourcePool()->getResources<QnLayoutResource>(), force);
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

    QnLayoutResourceList existing = alreadyExistingLayouts(resourcePool(), dialog->name(), user->getId());
    if (!canRemoveLayouts(existing))
    {
        ui::messages::Resources::layoutAlreadyExists(mainWindowWidget());
        return;
    }

    if (!existing.isEmpty())
    {
        bool allAreLocal = boost::algorithm::all_of(existing,
            [](const QnLayoutResourcePtr& layout)
            {
                return layout->hasFlags(Qn::local);
            });

        if (!allAreLocal && !ui::messages::Resources::overrideLayout(mainWindowWidget()))
            return;

        removeLayouts(existing);
    }

    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setIdUnsafe(QnUuid::createUuid());
    layout->setName(dialog->name());
    layout->setParentId(user->getId());
    resourcePool()->addResource(layout);

    appContext()->currentSystemContext()->layoutSnapshotManager()->save(layout);

    menu()->trigger(action::OpenInNewTabAction, layout);
}

void LayoutsHandler::at_saveLayoutAction_triggered()
{
    saveLayout(menu()->currentParameters(sender()).resource().dynamicCast<QnLayoutResource>());
}

void LayoutsHandler::at_saveCurrentLayoutAction_triggered()
{
    saveLayout(workbench()->currentLayout()->resource());
}

void LayoutsHandler::at_saveLayoutForCurrentUserAsAction_triggered()
{
    saveLayoutAs(
        menu()->currentParameters(sender()).resource().dynamicCast<QnLayoutResource>(),
        context()->user()
    );
}

void LayoutsHandler::at_saveLayoutAsAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    saveLayoutAs(
        parameters.resource().dynamicCast<QnLayoutResource>(),
        parameters.argument<QnUserResourcePtr>(Qn::UserResourceRole)
    );
}

void LayoutsHandler::at_saveLayoutAsCloudAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    saveLayoutAsCloud(parameters.resource().dynamicCast<QnLayoutResource>());
}

void LayoutsHandler::at_saveCurrentLayoutAsAction_triggered()
{
    saveLayoutAs(
        workbench()->currentLayout()->resource(),
        context()->user()
    );
}

void LayoutsHandler::at_saveCurrentLayoutAsCloudAction_triggered()
{
    saveLayoutAsCloud(workbench()->currentLayout()->resource());
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
    if (layouts.empty())
        return;

    QnWorkbenchLayoutList layoutsToClose = workbench()->layouts();
    foreach(QnWorkbenchLayout *layout, layouts)
        layoutsToClose.removeOne(layout);

    closeLayouts(layoutsToClose);
}

void LayoutsHandler::at_removeFromServerAction_triggered()
{
    auto layouts = menu()->currentParameters(sender()).resources().filtered<QnLayoutResource>();

    QnLayoutResourceList canAutoDelete;
    QnLayoutResourceList shared;
    QHash<QnUserResourcePtr, QnLayoutResourceList> common;
    for (const auto& layout : layouts)
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
    QnWorkbenchLayout *layout = qnWorkbenchLayoutsFactory->create(this);

    const auto parameters = menu()->currentParameters(sender());

    if (parameters.hasArgument(Qn::LayoutSyncStateRole))
    {
        const auto syncState = parameters.argument(Qn::LayoutSyncStateRole);
        layout->setData(Qn::LayoutSyncStateRole, syncState);
    }

    layout->setName(generateUniqueLayoutName(resourcePool(), context()->user(), tr("New Layout"), tr("New Layout %1")));
    workbench()->addLayout(layout);

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

bool LayoutsHandler::tryClose(bool force)
{
    return closeAllLayouts(force);
}

void LayoutsHandler::forcedUpdate()
{
    //do nothing
}

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
