// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_action_handler.h"

#include <chrono>

#include <QtCore/QTimer>
#include <QtCore/QWeakPointer>
#include <QtGui/QAction>

#include <api/server_rest_connection.h>
#include <client/client_globals.h>
#include <client/client_message_processor.h>
#include <core/resource/camera_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/layout_reader.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/counter.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/metatypes.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/core/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx/vms/client/core/resource/unified_resource_pool.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameter_types.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/radass/radass_resource_manager.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/resource/layout_resource_helpers.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/handlers/notification_action_handler.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_event_rules_manager.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <nx_ec/managers/abstract_resource_manager.h>
#include <ui/dialogs/layout_name_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/extensions/workbench_layout_change_validator.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/handlers/workbench_videowall_handler.h> //< TODO: #sivanov Dependencies.
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_state_manager.h>
#include <utils/common/delayed.h>
#include <utils/common/delete_later.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>

using namespace std::chrono;
using namespace nx::vms::common::system_health;

namespace nx::vms::client::desktop {

namespace {

/**
 * @brief alreadyExistingLayouts    Check if layouts with same name already exist.
 * @param name                      Suggested new name.
 * @param parentId                  Layout owner.
 * @param layout                    Layout that we want to rename (if any).
 * @return                          List of existing layouts with same name.
 */
core::LayoutResourceList alreadyExistingLayouts(
    QnResourcePool* resourcePool,
    const QString& name,
    const nx::Uuid& parentId,
    const core::LayoutResourcePtr& layout = core::LayoutResourcePtr())
{
    core::LayoutResourceList result;
    for (const auto& existingLayout:
        resourcePool->getResourcesByParentId(parentId).filtered<core::LayoutResource>())
    {
        if (existingLayout == layout)
            continue;

        if (existingLayout->getName().toLower() != name.toLower())
            continue;

        result << existingLayout;
    }
    return result;
}

QSet<QnResourcePtr> localLayoutResources(QnResourcePool* resourcePool,
    const common::LayoutItemDataMap& items)
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

QSet<QnResourcePtr> localLayoutResources(const core::LayoutResourcePtr& layout)
{
    return localLayoutResources(layout->resourcePool(), layout->getItems());
}

bool hasCrossSystemItems(const core::LayoutResourcePtr& layout)
{
    const auto currentCloudSystemId = appContext()->currentSystemContext()->globalSettings()
        ->cloudSystemId();

    auto items = layout->getItems();
    return std::any_of(items.cbegin(), items.cend(),
        [currentCloudSystemId](const common::LayoutItemData& item)
        {
            return core::isCrossSystemResource(item.resource)
                && core::crossSystemResourceSystemId(item.resource) != currentCloudSystemId;
        });
}

void saveCloudLayoutRetryCallback(bool success, const core::LayoutResourcePtr& layout)
{
    if (success || !layout->canBeSaved())
        return;

    const auto kSaveAttemptDelay = 5s;
    QTimer::singleShot(kSaveAttemptDelay,
        [layoutWeakPtr = QWeakPointer<core::LayoutResource>(layout)]()
        {
            if (auto layout = layoutWeakPtr.toStrongRef())
                layout->saveAsync(saveCloudLayoutRetryCallback);
        });
};

void postAcceptIntercomCall(
    SystemContext* system,
    nx::Uuid intercomId,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto serverApi = system->connectedServerApi();
    if (!NX_ASSERT(serverApi))
        return;

    serverApi->sendRequest<rest::ServerConnection::ErrorOrEmpty>(
        system->getSessionTokenHelper(),
        nx::network::http::Method::post,
        nx::format("/rest/v4/devices/%1/intercom/acceptCall", intercomId.toSimpleString()),
        nx::network::rest::Params(),
        /*body*/ QByteArray(),
        [intercomId](
            bool success,
            rest::Handle /*requestId*/,
            const rest::ServerConnection::ErrorOrEmpty& result)
        {
            if (success)
                return;

            QString error = nx::format(
                "The call acceptance operation for the %1 intercom has failed", intercomId);

            if (!result)
            {
                error += nx::format(": %1", result.error().errorId);
                if (!result.error().errorString.isEmpty())
                    error += nx::format(", %1", result.error().errorString);
            }
            NX_WARNING(NX_SCOPE_TAG, error);
        },
        executor);
}

} // namespace

LayoutActionHandler::LayoutActionHandler(WindowContext* windowContext, QObject* parent):
    QObject(parent),
    WindowContextAware(windowContext)
{
    auto getCurrentLayout =
        [this]() { return workbench()->currentLayout()->resource(); };
    auto getLayoutFromParameters =
        [this]()
        {
            return menu()->currentParameters(sender()).resource().dynamicCast<core::LayoutResource>();
        };

    connect(action(menu::NewUserLayoutAction), &QAction::triggered, this,
        &LayoutActionHandler::at_newUserLayoutAction_triggered);

    connect(action(menu::SaveLayoutAction), &QAction::triggered, this,
        [this, getLayoutFromParameters]() { saveLayout(getLayoutFromParameters()); });

    connect(action(menu::SaveCurrentLayoutAction), &QAction::triggered, this,
        [this, getCurrentLayout]() { saveLayout(getCurrentLayout()); });

    connect(action(menu::SaveLayoutAsCloudAction), &QAction::triggered, this,
        [this, getLayoutFromParameters]() { saveLayoutAsCloud(getLayoutFromParameters()); });

    connect(action(menu::SaveCurrentLayoutAsCloudAction), &QAction::triggered, this,
        [this, getCurrentLayout]() { saveLayoutAsCloud(getCurrentLayout());});

    connect(action(menu::SaveLayoutAsAction), &QAction::triggered, this,
        [this, getLayoutFromParameters]() { saveLayoutAs(getLayoutFromParameters()); });

    connect(action(menu::SaveCurrentLayoutAsAction), &QAction::triggered, this,
        [this, getCurrentLayout]() { saveLayoutAs(getCurrentLayout()); });

    connect(action(menu::ConvertLayoutToSharedAction), &QAction::triggered, this,
        [this, getLayoutFromParameters]() { convertLayoutToShared(getLayoutFromParameters()); });

    connect(action(menu::CloseLayoutAction), &QAction::triggered, this,
        &LayoutActionHandler::at_closeLayoutAction_triggered);
    connect(action(menu::CloseAllButThisLayoutAction), &QAction::triggered, this,
        &LayoutActionHandler::at_closeAllButThisLayoutAction_triggered);
    connect(action(menu::RemoveFromServerAction), &QAction::triggered, this,
        &LayoutActionHandler::at_removeFromServerAction_triggered);
    connect(action(menu::OpenNewTabAction), &QAction::triggered, this,
        &LayoutActionHandler::at_openNewTabAction_triggered);
    connect(action(menu::ForgetLayoutPasswordAction), &QAction::triggered, this,
        &LayoutActionHandler::at_forgetLayoutPasswordAction_triggered);
    connect(action(menu::RenameResourceAction), &QAction::triggered, this,
        &LayoutActionHandler::onRenameResourceAction);

    connect(action(menu::RemoveLayoutItemAction), &QAction::triggered, this,
        &LayoutActionHandler::at_removeLayoutItemAction_triggered);
    connect(action(menu::RemoveLayoutItemFromSceneAction), &QAction::triggered, this,
        &LayoutActionHandler::at_removeLayoutItemFromSceneAction_triggered);

    connect(action(menu::OpenInNewTabAction), &QAction::triggered, this,
        &LayoutActionHandler::at_openInNewTabAction_triggered);
    connect(action(menu::MonitorInNewTabAction), &QAction::triggered, this,
        &LayoutActionHandler::at_openInNewTabAction_triggered);

    connect(action(menu::OpenIntercomLayoutAction), &QAction::triggered, this,
        &LayoutActionHandler::at_openIntercomLayoutAction_triggered);
    connect(action(menu::OpenMissedCallIntercomLayoutAction), &QAction::triggered, this,
        &LayoutActionHandler::at_openMissedCallIntercomLayoutAction_triggered);

    connect(appContext()->unifiedResourcePool(), &core::UnifiedResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            auto layouts = resources.filtered<core::LayoutResource>();
            if (!layouts.empty())
                workbench()->removeLayouts(layouts);

            // Safety measure to ensure client will never have an empty workbench state.
            if (system()->clientMessageProcessor()
                    ->connectionStatus()->state() == QnConnectionState::Ready
                && workbench()->layouts().empty()
                && !workbench()->inClearProcess())
            {
                menu()->trigger(menu::OpenNewTabAction);
            }
        });

    const auto cachingController = qobject_cast<CachingAccessController*>(
        system()->accessController());
    if (NX_ASSERT(cachingController))
    {
        connect(cachingController, &CachingAccessController::permissionsChanged, this,
            [this](const QnResourcePtr& resource)
            {
                const auto layoutResource = resource.dynamicCast<core::LayoutResource>();
                if (!layoutResource)
                    return;

                const auto accessController = system()->accessController();

                if (layoutResource->layoutType() == core::LayoutResource::LayoutType::videoWall
                    && accessController->hasPermissions(
                        layoutResource->getParentResource(), Qn::ReadPermission))
                {
                    // Videowall handler shall handle this.
                    return;
                }

                // Remove layout if current user lost access to it.
                if (!system()->accessController()->hasPermissions(
                    layoutResource, Qn::ReadPermission))
                {
                    workbench()->removeLayout(layoutResource);

                    if (workbench()->layouts().empty())
                        menu()->trigger(menu::OpenNewTabAction);
                }
            });
    }
}

LayoutActionHandler::~LayoutActionHandler()
{
}

void LayoutActionHandler::at_forgetLayoutPasswordAction_triggered()
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
    layout->storeSnapshot();
}

void LayoutActionHandler::renameLayout(const core::LayoutResourcePtr &layout, const QString &newName)
{
    auto systemContext = SystemContext::fromResource(layout);
    if (!NX_ASSERT(systemContext))
        return;

    core::LayoutResourceList existing = alreadyExistingLayouts(
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

    const bool changed = layout->isChanged();

    layout->setName(newName);

    if (!changed)
        layout->saveAsync();
}

void LayoutActionHandler::saveLayout(const core::LayoutResourcePtr& layout)
{
    if (!layout)
        return;

    // This scenario is not very actual as it can be caused only if user places cross-system
    // cameras on a local layout using api.
    if (!layout->isCrossSystem() && hasCrossSystemItems(layout))
    {
        appContext()->cloudLayoutsManager()->convertLocalLayout(layout);
        return;
    }

    auto systemContext = SystemContext::fromResource(layout);
    if (!NX_ASSERT(systemContext))
        return;

    if (!layout->canBeSaved())
        return;

    if (!ResourceAccessManager::hasPermissions(layout, Qn::SavePermission))
        return;

    if (layout->isFile())
    {
        menu()->trigger(menu::SaveLocalLayoutAction, layout);
        return;
    }

    if (layout->isCrossSystem())
    {
        layout->saveAsync(saveCloudLayoutRetryCallback);
        return;
    }

    const auto change = calculateLayoutChange(layout);
    const auto layoutOwner = layout->getParentResource();
    const bool revertOnCancel = workbench()->currentLayout()->resource() != layout;

    if (confirmLayoutChange(change, layoutOwner))
    {
        layout->saveAsync();
    }
    else if (revertOnCancel)
    {
        layout->restoreFromSnapshot();
    }
}

void LayoutActionHandler::saveLayoutAs(const core::LayoutResourcePtr& layout)
{
    if (!NX_ASSERT(layout))
        return;

    if (layout->isCrossSystem())
        saveCloudLayoutAs(layout);
    else
        saveRemoteLayoutAs(layout);
}

void LayoutActionHandler::saveRemoteLayoutAs(const core::LayoutResourcePtr& layout)
{
    QnUserResourcePtr user = system()->user();
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

    QString proposedName =
        hasSavePermission ? layout->getName() : generateUniqueLayoutName(system()->user());

    dialog->setName(proposedName);
    setHelpTopic(dialog.data(), HelpTopic::Id::SaveLayout);

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
        core::LayoutResourcePtr excludingSelfLayout =
            hasSavePermission ? layout : core::LayoutResourcePtr();
        core::LayoutResourceList existing = alreadyExistingLayouts(
            system()->resourcePool(),
            name,
            user->getId(),
            excludingSelfLayout);
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

    core::LayoutResource::ItemsRemapHash newUuidByOldUuid;
    core::LayoutResourcePtr newLayout = layout->clone(&newUuidByOldUuid);
    newLayout->setName(name);
    newLayout->setParentId(user->getId());
    system()->resourcePool()->addResource(newLayout);

    const bool isCurrent = (layout == workbench()->currentLayout()->resource());

    // We can "Save as" temporary layouts like Alarm layout or Preview Search or Audit Trail.
    const bool isTemporaryLayout = !layout->systemContext();

    const auto systemContext = isTemporaryLayout
        ? appContext()->currentSystemContext()
        : SystemContext::fromResource(layout);

    if (!NX_ASSERT(systemContext))
        return;

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
            layout->restoreFromSnapshot();
    }

    newLayout->saveAsync();

    if (shouldDelete)
        removeLayouts(core::LayoutResourceList() << layout);
}

void LayoutActionHandler::saveLayoutAsCloud(const core::LayoutResourcePtr& layout)
{
    if (!NX_ASSERT(layout))
        return;

    NX_ASSERT(!layout->isCrossSystem());

    auto dialog = createSelfDestructingDialog<QnLayoutNameDialog>(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, mainWindowWidget());
    dialog->setWindowTitle(tr("Save Layout As Cloud"));
    dialog->setText(tr("Enter Layout Name:"));

    const QString proposedName = layout->getName();
    dialog->setName(proposedName);

    connect(
        dialog,
        &QnLayoutNameDialog::accepted,
        this,
        [this, dialog, layout]
        {
            if (dialog->clickedButton() != QDialogButtonBox::Save)
                return;

            const QString name = dialog->name();

            auto cloudResourcesPool = appContext()->cloudLayoutsSystemContext()->resourcePool();

            core::LayoutResourceList existing = cloudResourcesPool->getResources<core::LayoutResource>(
                [suggestedName = name.toLower()](const core::LayoutResourcePtr& layout)
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
        });

    dialog->show();
}

void LayoutActionHandler::saveCloudLayoutAs(const core::LayoutResourcePtr& layout)
{
    if (!NX_ASSERT(layout->isCrossSystem()))
        return;

    auto dialog = createSelfDestructingDialog<QnLayoutNameDialog>(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, mainWindowWidget());

    dialog->setWindowTitle(tr("Save Layout As"));
    dialog->setText(tr("Enter Layout Name:"));

    const QString proposedName = layout->getName();
    dialog->setName(proposedName);

    connect(
        dialog,
        &QnLayoutNameDialog::accepted,
        this,
        [this, dialog, proposedName, layout]
        {
            if (dialog->clickedButton() != QDialogButtonBox::Save)
                return;

            const QString name = dialog->name();

            // User press "Save As" and enters the same name as this layout already has.
            if (name == proposedName)
            {
                saveLayout(layout);
                return;
            }

            auto cloudResourcesPool = appContext()->cloudLayoutsSystemContext()->resourcePool();

            core::LayoutResourceList existing = cloudResourcesPool->getResources<core::LayoutResource>(
                [suggestedName = name.toLower()](const core::LayoutResourcePtr& layout)
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
            NX_ASSERT(layout->isCrossSystem());
            cloudLayout->addFlags(Qn::local);
            cloudLayout->setName(name);
            appContext()->cloudLayoutsSystemContext()->resourcePool()->addResource(cloudLayout);
            saveLayout(cloudLayout);
            workbench()->replaceLayout(layout, cloudLayout);
        });

    dialog->show();
}

void LayoutActionHandler::convertLayoutToShared(const core::LayoutResourcePtr& layout)
{
    const auto user = system()->accessController()->user();
    if (!NX_ASSERT(layout && user && layout->getParentId() == user->getId()))
        return;

    if (!ui::messages::Resources::convertLayoutToShared(mainWindowWidget()))
        return;

    layout->setParentId({});
    layout->saveAsync();

    // Re-select the layout in Resource Tree.
    menu()->trigger(menu::SelectNewItemAction, layout);
}

void LayoutActionHandler::removeLayoutItems(const LayoutItemIndexList& items, bool autoSave)
{
    if (items.size() > 1)
    {
        const auto layout = items.first().layout();
        const bool isShowreel = isShowreelReviewLayout(layout);
        const auto resources = menu::ParameterTypes::resources(items);

        const bool confirm = isShowreel
            ? ui::messages::Resources::removeItemsFromShowreel(mainWindowWidget(), resources)
            : ui::messages::Resources::removeItemsFromLayout(mainWindowWidget(), resources);

        if (!confirm)
            return;
    }

    QList<nx::Uuid> orphanedUuids;
    QSet<core::LayoutResourcePtr> layouts;
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
        for (const nx::Uuid &uuid : orphanedUuids)
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
            menu()->trigger(menu::SaveLayoutAction, layout);
    }
}

LayoutActionHandler::LayoutChange LayoutActionHandler::calculateLayoutChange(
    const core::LayoutResourcePtr& layout)
{
    auto systemContext = SystemContext::fromResource(layout);
    if (!NX_ASSERT(systemContext))
        return {};

    LayoutChange result;
    result.layout = layout;

    /* Share added resources. */
    const auto& snapshot = layout->snapshot();

    // Check only Resources from the same System Context as the Layout. Cross-system Resources
    // cannot be available through Shared Layouts.
    auto oldResources = localLayoutResources(systemContext->resourcePool(), snapshot.items);
    auto newResources = localLayoutResources(layout);

    result.added = (newResources - oldResources).values();
    result.removed = (oldResources - newResources).values();

    return result;
}

bool LayoutActionHandler::confirmLayoutChange(
    const LayoutChange& change,
    const QnResourcePtr& layoutOwner)
{
    if (!NX_ASSERT(system()->user(), "Should never ask for layout saving when offline"))
        return false;

    // Never ask for own layouts.
    if (layoutOwner == system()->user())
        return true;

    // Ask confirmation if user will lose some of cameras due to videowall layout change.
    if (layoutOwner && layoutOwner->hasFlags(Qn::videowall))
        return confirmChangeVideoWallLayout(change);

    // Never ask for auto-generated (e.g. preview search) layouts.
    if (change.layout->isServiceLayout())
        return true;

    // Check shared layout.
    if (change.layout->isShared())
        return confirmChangeSharedLayout(change);

    // Never ask for intercom layouts.
    if (change.layout->layoutType() == core::LayoutResource::LayoutType::intercom)
        return true;

    NX_ASSERT(false, "Editing of private layouts of other users is no longer supported.");
    return false;
}

bool LayoutActionHandler::confirmChangeSharedLayout(const LayoutChange& /*change*/)
{
    return ui::messages::Resources::sharedLayoutEdit(mainWindowWidget());
}

bool LayoutActionHandler::confirmChangeVideoWallLayout(const LayoutChange& change)
{
    QnWorkbenchLayoutsChangeValidator validator(workbenchContext());
    return validator.confirmChangeVideoWallLayout(change.layout, change.removed);
}

bool LayoutActionHandler::canRemoveLayouts(const core::LayoutResourceList &layouts)
{
    return std::all_of(layouts.cbegin(), layouts.cend(),
        [](const core::LayoutResourcePtr& layout)
        {
            return ResourceAccessManager::hasPermissions(layout, Qn::RemovePermission);
        });
}

void LayoutActionHandler::removeLayouts(const core::LayoutResourceList &layouts)
{
    if (layouts.isEmpty())
        return;

    if (!canRemoveLayouts(layouts))
        return;

    core::LayoutResourceList remoteResources;
    for (const core::LayoutResourcePtr& layout: layouts)
    {
        NX_ASSERT(!layout->isFile());
        if (layout->isFile())
            continue;

        if (layout->isCrossSystem())
            appContext()->cloudLayoutsManager()->deleteLayout(layout);
        else if (layout->hasFlags(Qn::local))
            system()->resourcePool()->removeResource(layout); /*< This one can be simply deleted from resource pool. */
        else
            remoteResources << layout;
    }

    for (const auto& resource: remoteResources)
        qnResourcesChangesManager->deleteResource(resource);
}

bool LayoutActionHandler::closeLayouts(const QnWorkbenchLayoutList& layouts)
{
    core::LayoutResourceList resources;
    for (auto layout: layouts)
        resources.push_back(layout->resource());

    return closeLayouts(resources);
}

bool LayoutActionHandler::closeLayouts(const core::LayoutResourceList& resources)
{
    if (resources.empty())
        return true;

    workbench()->removeLayouts(resources);
    if (workbench()->layouts().empty())
        menu()->trigger(menu::OpenNewTabAction);

    return true;
}

void LayoutActionHandler::openLayouts(
    const core::LayoutResourceList& layouts,
    const StreamSynchronizationState& playbackState,
    bool forceStateUpdate)
{
    if (!NX_ASSERT(!layouts.empty()))
        return;

    QnWorkbenchLayout* lastLayout = nullptr;
    for (const auto& layout: layouts)
    {
        auto wbLayout = workbench()->layout(layout);
        const bool updateState = !wbLayout || forceStateUpdate;

        if (!wbLayout)
            wbLayout = workbench()->addLayout(layout);

        if (updateState)
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
                if (forceStateUpdate || !item->data(Qn::ItemTimeRole).isValid())
                {
                    item->setData(Qn::ItemTimeRole,
                        state.timeUs == DATETIME_NOW ? DATETIME_NOW : state.timeUs / 1000);
                    item->setData(Qn::ItemPausedRole, state.speed == 0);
                    item->setData(Qn::ItemSpeedRole, state.speed);
                }
            }
        }

        // Explicitly set that we do not control videowall through this layout.
        wbLayout->setData(Qn::VideoWallItemGuidRole, QVariant::fromValue(nx::Uuid()));

        lastLayout = wbLayout;
    }
    workbench()->setCurrentLayout(lastLayout);
}

QString LayoutActionHandler::generateUniqueLayoutName(const QnUserResourcePtr& user) const
{
    QStringList usedNames;
    nx::Uuid parentId = user ? user->getId() : nx::Uuid();
    for (const auto& layout: system()->resourcePool()->getResources<core::LayoutResource>())
    {
        if (layout->isShared() || layout->getParentId() == parentId)
            usedNames.push_back(layout->getName());
    }

    return nx::utils::generateUniqueString(usedNames,
        tr("New Layout") + " 1",
        tr("New Layout") + " %1");
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void LayoutActionHandler::at_newUserLayoutAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    const auto user = system()->user();
    if (!user)
        return;

    auto dialog = createSelfDestructingDialog<QnLayoutNameDialog>(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, mainWindowWidget());
    dialog->setWindowTitle(tr("New Layout"));
    dialog->setText(tr("Enter the name of the layout to create:"));
    dialog->setName(generateUniqueLayoutName(user));

    connect(
        dialog,
        &QnLayoutNameDialog::accepted,
        this,
        [this, dialog, user]
        {
            onNewUserLayoutNameChoosen(dialog->name(), user);
        });

    dialog->show();
}

void LayoutActionHandler::at_closeLayoutAction_triggered()
{
    auto layouts = menu()->currentParameters(sender()).layouts();
    if (layouts.empty() && workbench()->layouts().size() > 1)
        layouts.push_back(workbench()->currentLayout());
    closeLayouts(layouts);
}

void LayoutActionHandler::at_closeAllButThisLayoutAction_triggered()
{
    QnWorkbenchLayoutList layouts = menu()->currentParameters(sender()).layouts();
    if (layouts.size() != 1)
        return;

    auto selectedLayout = layouts[0];
    workbench()->setCurrentLayout(selectedLayout);

    core::LayoutResourceList layoutsToClose;
    for (const auto& layout: workbench()->layouts())
        layoutsToClose.push_back(layout->resource());

    layoutsToClose.removeOne(selectedLayout->resource());
    closeLayouts(layoutsToClose);
}

void LayoutActionHandler::at_removeFromServerAction_triggered()
{
    auto layouts = menu()->currentParameters(sender()).resources().filtered<core::LayoutResource>();

    core::LayoutResourceList personalAndVideowallLayouts;
    core::LayoutResourceList sharedLayouts;
    for (const auto& layout: layouts)
    {
        NX_ASSERT(!layout->isFile());
        if (layout->isFile())
            continue;

        if (layout->isCrossSystem())
            personalAndVideowallLayouts << layout;
        else if (layout->isShared())
            sharedLayouts << layout;
        else
            personalAndVideowallLayouts << layout;
    }

    const auto personalLayouts = personalAndVideowallLayouts.filtered(
        [](const core::LayoutResourcePtr& layout)
        {
            return !layout->getParentResource().objectCast<QnVideoWallResource>();
        });

    if (ui::messages::Resources::deleteLayouts(mainWindowWidget(), sharedLayouts, personalLayouts))
    {
        removeLayouts(sharedLayouts);
        removeLayouts(personalAndVideowallLayouts);
    }
}

void LayoutActionHandler::onRenameResourceAction()
{
    const auto parameters = menu()->currentParameters(sender());

    if (const auto layout = parameters.resource().dynamicCast<core::LayoutResource>();
        layout && !layout->isFile())
    {
        QString name = parameters.argument<QString>(core::ResourceNameRole).trimmed();
        if (name == layout->getName())
            return;

        renameLayout(layout, name);
    }
}

void LayoutActionHandler::onNewUserLayoutNameChoosen(
    const QString& name, const QnUserResourcePtr& user)
{
    core::LayoutResourceList existing = alreadyExistingLayouts(
        system()->resourcePool(),
        name,
        user->getId());

    if (!canRemoveLayouts(existing))
    {
        ui::messages::Resources::layoutAlreadyExists(mainWindowWidget());
        return;
    }

    if (!existing.isEmpty())
    {
        bool allAreLocal = std::all_of(existing.cbegin(), existing.cend(),
            [](const core::LayoutResourcePtr& layout)
            {
                return layout->hasFlags(Qn::local);
            });

        if (!allAreLocal && !ui::messages::Resources::overrideLayout(mainWindowWidget()))
            return;

        removeLayouts(existing);
    }

    core::LayoutResourcePtr layout(new core::LayoutResource());
    layout->setIdUnsafe(nx::Uuid::createUuid());
    layout->setName(name);
    layout->setParentId(user->getId());
    system()->resourcePool()->addResource(layout);

    layout->saveAsync();

    menu()->trigger(menu::OpenInNewTabAction, layout);
}

void LayoutActionHandler::at_openNewTabAction_triggered()
{
    auto resource = core::LayoutResourcePtr(new core::LayoutResource());
    resource->setIdUnsafe(nx::Uuid::createUuid());
    resource->addFlags(Qn::local);
    resource->setName(generateUniqueLayoutName(system()->user()));
    if (system()->user())
        resource->setParentId(system()->user()->getId());

    {
        // Do not add resource on workbench layout if something was added there via resource pool.
        const auto count = workbench()->layouts().size();
        system()->resourcePool()->addResource(resource);
        if (workbench()->layouts().size() > count)
            return;
    }
    QnWorkbenchLayout* layout = workbench()->addLayout(resource);

    const auto parameters = menu()->currentParameters(sender());
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

void LayoutActionHandler::at_removeLayoutItemAction_triggered()
{
    removeLayoutItems(menu()->currentParameters(sender()).layoutItems(), true);
}

void LayoutActionHandler::at_removeLayoutItemFromSceneAction_triggered()
{
    const auto layoutItems = menu()->currentParameters(sender()).layoutItems();
    removeLayoutItems(layoutItems, false);
}

void LayoutActionHandler::at_openInNewTabAction_triggered()
{
    const auto isCameraWithFootage =
        [](const QnMediaResourceWidget* widget)
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

    common::LayoutItemDataList items;
    if (calledFromScene)
    {
        for (const auto& widget: parameters.widgets())
            items.push_back(widget->item()->data());
        items = core::LayoutResource::cloneItems(items);
    }

    const auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();

    // Update state depending on how the action was called.
    auto currentState = streamSynchronizer->state();
    bool forceStateUpdate = false;

    if (parameters.hasArgument(Qn::LayoutSyncStateRole))
    {
        currentState = parameters.argument<StreamSynchronizationState>(Qn::LayoutSyncStateRole);
        forceStateUpdate = (currentState.timeUs != DATETIME_INVALID);
    }
    else if (!currentMediaWidget
        || !isCameraWithFootage(currentMediaWidget)
        || currentMediaWidget->isPlayingLive())
    {
        // Switch to live if current item is not a camera or has no footage or playing live.
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
    if (action(menu::ToggleShowreelModeAction)->isChecked())
        menu()->trigger(menu::ToggleShowreelModeAction);

    const auto layouts = resources.filtered<core::LayoutResource>();
    if (!layouts.empty())
    {
        NX_ASSERT(!calledFromScene, "Layouts can not be passed from the scene");
        NX_ASSERT(resources.size() == layouts.size(), "Mixed resources are not expected here");
        openLayouts(layouts, currentState, forceStateUpdate);

        // Do not return in case mixed resource set is passed somehow.
    }

    const auto openable = resources.filtered(QnResourceAccessFilter::isOpenableInLayout);

    if (openable.empty())
        return;

    const bool hasCrossSystemResources = std::any_of(
        openable.cbegin(), openable.cend(),
        [](const QnResourcePtr& resource) { return resource->hasFlags(Qn::cross_system); });

    auto layout = hasCrossSystemResources
        ? core::LayoutResourcePtr(new core::CrossSystemLayoutResource())
        : core::LayoutResourcePtr(new core::LayoutResource());
    layout->setIdUnsafe(nx::Uuid::createUuid());
    layout->addFlags(Qn::local);

    if (hasCrossSystemResources)
    {
        appContext()->cloudLayoutsSystemContext()->resourcePool()->addResource(layout);
        layout->setName(generateUniqueLayoutName(/*user*/ {}));
    }
    else
    {
        if (system()->user())
            layout->setParentId(system()->user()->getId());
        system()->resourcePool()->addResource(layout);
        layout->setName(generateUniqueLayoutName(system()->user()));
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
        menu()->trigger(menu::DropResourcesAction, parameters);
    }
}

void LayoutActionHandler::at_openIntercomLayoutAction_triggered()
{
    at_openInNewTabAction_triggered();

    const menu::Parameters parameters = menu()->currentParameters(sender());
    const auto intercomId = parameters.argument<nx::Uuid>(nx::vms::client::core::UuidRole);
    NX_ASSERT(!intercomId.isNull());

    postAcceptIntercomCall(system(), intercomId, thread());
}

void LayoutActionHandler::at_openMissedCallIntercomLayoutAction_triggered()
{
    at_openInNewTabAction_triggered();

    const menu::Parameters parameters = menu()->currentParameters(sender());
    const auto notificationId = parameters.argument<nx::Uuid>(Qn::ItemUuidRole);
    NX_ASSERT(!notificationId.isNull());

    auto message = nx::vms::api::SiteHealthMessage{
        .type = MessageType::showMissedCallInformer,
        .active = false,
        .resourceIds = {notificationId},
    };

    executeDelayedParented(
        [this, message = std::move(message)]
        {
            windowContext()->notificationActionHandler()->removeNotification(message);
        },
        this);
}

} // namespace nx::vms::client::desktop
