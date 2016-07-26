#include "workbench_layouts_handler.h"

#include <api/app_server_connection.h>

#include <client/client_globals.h>
#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource_management/resources_changes_manager.h>

#include <nx_ec/dummy_handler.h>
#include <nx_ec/managers/abstract_layout_manager.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/dialogs/layout_name_dialog.h>
#include <ui/dialogs/resource_list_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/handlers/workbench_export_handler.h>     //TODO: #GDM dependencies
#include <ui/workbench/handlers/workbench_videowall_handler.h>  //TODO: #GDM dependencies
#include <ui/workbench/workbench_state_manager.h>

#include <utils/common/counter.h>
#include <nx/utils/string.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_value_rollback.h>

namespace {
QString generateUniqueLayoutName(const QnUserResourcePtr &user, const QString &defaultName, const QString &nameTemplate)
{
    QStringList usedNames;
    QnUuid parentId = user ? user->getId() : QnUuid();
    for (const auto layout : qnResPool->getResources<QnLayoutResource>())
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
QnLayoutResourceList alreadyExistingLayouts(const QString &name, const QnUuid &parentId, const QnLayoutResourcePtr &layout = QnLayoutResourcePtr())
{
    QnLayoutResourceList result;
    for (const auto& existingLayout : qnResPool->getResourcesWithParentId(parentId).filtered<QnLayoutResource>())
    {
        if (existingLayout == layout)
            continue;

        if (existingLayout->getName().toLower() != name.toLower())
            continue;

        result << existingLayout;
    }
    return result;
}

QSet<QnResourcePtr> layoutResources(const QnLayoutItemDataMap& items)
{
    QSet<QnResourcePtr> result;
    for (const auto& item : items)
    {
        if (auto resource = qnResPool->getResourceByDescriptor(item.resource))
            result << resource;
    }
    return result;
};

}


QnWorkbenchLayoutsHandler::QnWorkbenchLayoutsHandler(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_workbenchStateDelegate(new QnBasicWorkbenchStateDelegate<QnWorkbenchLayoutsHandler>(this)),
    m_closingLayouts(false)
{
    connect(action(QnActions::NewUserLayoutAction),                 &QAction::triggered, this, &QnWorkbenchLayoutsHandler::at_newUserLayoutAction_triggered);
    connect(action(QnActions::SaveLayoutAction),                    &QAction::triggered, this, &QnWorkbenchLayoutsHandler::at_saveLayoutAction_triggered);
    connect(action(QnActions::SaveLayoutAsAction),                  &QAction::triggered, this, &QnWorkbenchLayoutsHandler::at_saveLayoutAsAction_triggered);
    connect(action(QnActions::SaveLayoutForCurrentUserAsAction),    &QAction::triggered, this, &QnWorkbenchLayoutsHandler::at_saveLayoutForCurrentUserAsAction_triggered);
    connect(action(QnActions::SaveCurrentLayoutAction),             &QAction::triggered, this, &QnWorkbenchLayoutsHandler::at_saveCurrentLayoutAction_triggered);
    connect(action(QnActions::SaveCurrentLayoutAsAction),           &QAction::triggered, this, &QnWorkbenchLayoutsHandler::at_saveCurrentLayoutAsAction_triggered);
    connect(action(QnActions::CloseLayoutAction),                   &QAction::triggered, this, &QnWorkbenchLayoutsHandler::at_closeLayoutAction_triggered);
    connect(action(QnActions::CloseAllButThisLayoutAction),         &QAction::triggered, this, &QnWorkbenchLayoutsHandler::at_closeAllButThisLayoutAction_triggered);
    connect(action(QnActions::RemoveFromServerAction),              &QAction::triggered, this, &QnWorkbenchLayoutsHandler::at_removeFromServerAction_triggered);
    connect(action(QnActions::ShareLayoutAction),                   &QAction::triggered, this, &QnWorkbenchLayoutsHandler::at_shareLayoutAction_triggered);
    connect(action(QnActions::StopSharingLayoutAction),             &QAction::triggered, this, &QnWorkbenchLayoutsHandler::at_stopSharingLayoutAction_triggered);
    connect(action(QnActions::OpenNewTabAction),                    &QAction::triggered, this, &QnWorkbenchLayoutsHandler::at_openNewTabAction_triggered);

    /* We're using queued connection here as modifying a field in its change notification handler may lead to problems. */
    connect(workbench(), &QnWorkbench::layoutsChanged, this, &QnWorkbenchLayoutsHandler::at_workbench_layoutsChanged, Qt::QueuedConnection);
}

QnWorkbenchLayoutsHandler::~QnWorkbenchLayoutsHandler()
{
}

ec2::AbstractECConnectionPtr QnWorkbenchLayoutsHandler::connection2() const
{
    return QnAppServerConnectionFactory::getConnection2();
}

void QnWorkbenchLayoutsHandler::renameLayout(const QnLayoutResourcePtr &layout, const QString &newName)
{
    QnLayoutResourceList existing = alreadyExistingLayouts(newName, layout->getParentId(), layout);
    if (!canRemoveLayouts(existing))
    {
        QnMessageBox::warning(
            mainWindow(),
            tr("Layout already exists."),
            tr("A layout with the same name already exists. You do not have the rights to overwrite it.")
        );
        return;
    }

    if (!existing.isEmpty())
    {
        if (askOverrideLayout(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, QDialogButtonBox::Cancel) == QDialogButtonBox::Cancel)
            return;
        removeLayouts(existing);
    }

    bool changed = snapshotManager()->isChanged(layout);

    layout->setName(newName);

    if (!changed)
        snapshotManager()->save(layout, [this](bool success, const QnLayoutResourcePtr &layout) { at_layout_saved(success, layout); });
}

void QnWorkbenchLayoutsHandler::saveLayout(const QnLayoutResourcePtr &layout)
{
    if (!layout)
        return;

    if (!snapshotManager()->isSaveable(layout))
        return;

    if (!accessController()->hasPermissions(layout, Qn::SavePermission))
        return;

    if (layout->isFile())
    {
        bool isReadOnly = !accessController()->hasPermissions(layout, Qn::WritePermission);
        QnWorkbenchExportHandler *exportHandler = context()->instance<QnWorkbenchExportHandler>();
        exportHandler->saveLocalLayout(layout, isReadOnly, true); // overwrite layout file
    }
    else if (!layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull())
    {
        //TODO: #GDM #VW #LOW refactor common code to common place
        if (context()->instance<QnWorkbenchVideoWallHandler>()->saveReviewLayout(layout, [this, layout](int reqId, ec2::ErrorCode errorCode)
        {
            snapshotManager()->setFlags(layout, snapshotManager()->flags(layout) & ~Qn::ResourceIsBeingSaved);
            at_layout_saved(errorCode == ec2::ErrorCode::ok, layout);
            if (errorCode != ec2::ErrorCode::ok)
                return;
            snapshotManager()->setFlags(layout, snapshotManager()->flags(layout) & ~Qn::ResourceIsChanged);
        }))
            snapshotManager()->setFlags(layout, snapshotManager()->flags(layout) | Qn::ResourceIsBeingSaved);
    }
    else
    {
        //TODO: #GDM #Common check existing layouts.
        //TODO: #GDM #Common all remotes layout checking and saving should be done in one place

        auto change = calculateLayoutChange(layout);

        auto owner = layout->getParentResource().dynamicCast<QnUserResource>();
        if (owner && qnResourceAccessManager->userRole(owner) == Qn::UserRole::CustomPermissions)
            grantAccessRightsForUser(owner, change);
        //TODO: #GDM #access Grant access rights for groups?

        //TODO: #GDM what if we've been disconnected while confirming?
        if (confirmLayoutChange(change))
            snapshotManager()->save(layout, [this](bool success, const QnLayoutResourcePtr &layout) { at_layout_saved(success, layout); });
        else
            snapshotManager()->restore(layout);
    }
}

void QnWorkbenchLayoutsHandler::saveLayoutAs(const QnLayoutResourcePtr &layout, const QnUserResourcePtr &user)
{
    if (!layout || !user)
        return;

    if (layout->isFile())
    {
        context()->instance<QnWorkbenchExportHandler>()->doAskNameAndExportLocalLayout(layout->getLocalRange(), layout, Qn::LayoutLocalSaveAs);
        return;
    }

    if (!layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull())
        return;

    const QnResourcePtr layoutOwnerUser = layout->getParentResource();
    bool hasSavePermission = accessController()->hasPermissions(layout, Qn::SavePermission);

    QString name = menu()->currentParameters(sender()).argument<QString>(Qn::ResourceNameRole).trimmed();
    if (name.isEmpty())
    {
        QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Save | QDialogButtonBox::Cancel, mainWindow()));
        dialog->setWindowTitle(tr("Save Layout As"));
        dialog->setText(tr("Enter Layout Name:"));

        QString proposedName = hasSavePermission
            ? layout->getName()
            : generateUniqueLayoutName(context()->user(), layout->getName(), layout->getName() + lit(" %1"));

        dialog->setName(proposedName);
        setHelpTopic(dialog.data(), Qn::SaveLayout_Help);

        QDialogButtonBox::StandardButton button = QDialogButtonBox::Cancel;
        do
        {
            if (!dialog->exec())
                return;

            /* Check if we were disconnected (server shut down) while the dialog was open. */
            if (!context()->user())
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

                switch (askOverrideLayout(QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel, QDialogButtonBox::Yes))
                {
                    case QDialogButtonBox::Cancel:
                        return;
                    case QDialogButtonBox::Yes:
                        saveLayout(layout);
                        return;
                    default:
                        continue;
                }
            }

            /* Check if we have rights to overwrite the layout */
            QnLayoutResourcePtr excludingSelfLayout = hasSavePermission ? layout : QnLayoutResourcePtr();
            QnLayoutResourceList existing = alreadyExistingLayouts(name, user->getId(), excludingSelfLayout);
            if (!canRemoveLayouts(existing))
            {
                QnMessageBox::warning(
                    mainWindow(),
                    tr("Layout already exists."),
                    tr("A layout with the same name already exists. You do not have the rights to overwrite it.")
                );
                dialog->setName(proposedName);
                continue;
            }

            button = QDialogButtonBox::Yes;
            if (!existing.isEmpty())
            {
                button = askOverrideLayout(QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel, QDialogButtonBox::Yes);
                if (button == QDialogButtonBox::Cancel)
                    return;
                if (button == QDialogButtonBox::Yes)
                {
                    removeLayouts(existing);
                }
            }
        } while (button != QDialogButtonBox::Yes);
    }
    else
    {
        QnLayoutResourceList existing = alreadyExistingLayouts(name, user->getId(), layout);
        if (!canRemoveLayouts(existing))
        {
            QnMessageBox::warning(
                mainWindow(),
                tr("Layout already exists."),
                tr("A layout with the same name already exists. You do not have the rights to overwrite it.")
            );
            return;
        }

        if (!existing.isEmpty())
        {
            if (askOverrideLayout(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, QDialogButtonBox::Cancel) == QDialogButtonBox::Cancel)
                return;
            removeLayouts(existing);
        }
    }

    QnLayoutResourcePtr newLayout;

    newLayout = QnLayoutResourcePtr(new QnLayoutResource());
    newLayout->setId(QnUuid::createUuid());
    newLayout->setName(name);
    newLayout->setParentId(user->getId());
    newLayout->setCellSpacing(layout->cellSpacing());
    newLayout->setCellAspectRatio(layout->cellAspectRatio());
    newLayout->setBackgroundImageFilename(layout->backgroundImageFilename());
    newLayout->setBackgroundOpacity(layout->backgroundOpacity());
    newLayout->setBackgroundSize(layout->backgroundSize());
    qnResPool->addResource(newLayout);

    QnLayoutItemDataList items = layout->getItems().values();
    QHash<QnUuid, QnUuid> newUuidByOldUuid;
    for (int i = 0; i < items.size(); i++)
    {
        QnUuid newUuid = QnUuid::createUuid();
        newUuidByOldUuid[items[i].uuid] = newUuid;
        items[i].uuid = newUuid;
    }
    for (int i = 0; i < items.size(); i++)
        items[i].zoomTargetUuid = newUuidByOldUuid.value(items[i].zoomTargetUuid, QnUuid());
    newLayout->setItems(items);

    const bool isCurrent = (layout == workbench()->currentLayout()->resource());
    bool shouldDelete = layout->hasFlags(Qn::local) &&
        (name == layout->getName() || isCurrent);

    /* If it is current layout, close it and open the new one instead. */
    if (isCurrent &&
        user == layoutOwnerUser)   //making current only new layout of current user
    {
        int index = workbench()->currentLayoutIndex();
        workbench()->insertLayout(new QnWorkbenchLayout(newLayout, this), index);
        workbench()->setCurrentLayoutIndex(index);
        workbench()->removeLayout(index + 1);

        // If current layout should not be deleted then roll it back
        if (!shouldDelete)
        {
            //(user == layoutOwnerUser) condition prevents clearing layout of another user (e.g., if copying layout from one user to another)
                //user - is an owner of newLayout. It is not required to be owner of layout
            snapshotManager()->restore(layout);
        }
    }

    snapshotManager()->save(newLayout, [this](bool success, const QnLayoutResourcePtr &layout) { at_layout_saved(success, layout); });
    if (shouldDelete)
        removeLayouts(QnLayoutResourceList() << layout);
}

QnWorkbenchLayoutsHandler::LayoutChange QnWorkbenchLayoutsHandler::calculateLayoutChange(const QnLayoutResourcePtr& layout)
{
    LayoutChange result;
    result.layout = layout;

    /* Share added resources. */
    auto snapshot = snapshotManager()->snapshot(layout);
    auto oldResources = layoutResources(snapshot.items);
    auto newResources = layoutResources(layout->getItems());

    result.added = (newResources - oldResources).toList();
    result.removed = (oldResources - newResources).toList();

    return result;
}

bool QnWorkbenchLayoutsHandler::confirmLayoutChange(const LayoutChange& change)
{
    NX_ASSERT(context()->user(), "Should never ask for layout saving when offline");
    if (!context()->user())
        return false;

    QnUuid ownerId = change.layout->getParentId();

    /* Never ask for own layouts. */
    if (ownerId == context()->user()->getId())
        return true;

    /* Check shared layout */
    if (change.layout->isShared())
        return confirmSharedLayoutChange(change);

    /* Do not save layout for non-existing user */
    QnUserResourcePtr owner = qnResPool->getResourceById<QnUserResource>(ownerId);
    if (!owner)
        return false;

    /* Do not warn if owner has access to all cameras anyway. */
    if (qnResourceAccessManager->hasGlobalPermission(owner, Qn::GlobalAccessAllMediaPermission))
        return true;

    auto role = qnResourceAccessManager->userRole(owner);
    switch (role)
    {
        case Qn::UserRole::CustomUserGroup:
            return confirmLayoutChangeForGroup(owner->userGroup(), change);
        case Qn::UserRole::CustomPermissions:
            return confirmLayoutChangeForUser(owner, change);
        default:
            break;
    }

    NX_ASSERT(false, "Should never get here");
    return true;
}

bool QnWorkbenchLayoutsHandler::confirmSharedLayoutChange(const LayoutChange& change)
{
    /* Check if user have already silenced this warning. */
    if (qnSettings->showOnceMessages().testFlag(Qn::ShowOnceMessage::SharedLayoutEdit))
        return true;

    /* Checking if custom users have access to this shared layout. */
    auto allUsers = qnResPool->getResources<QnUserResource>();
    auto accessibleToCustomUsers = std::any_of(
        allUsers.cbegin(), allUsers.cend(),
        [layout = change.layout](const QnUserResourcePtr& user)
        {
            return !qnResourceAccessManager->hasGlobalPermission(user, Qn::GlobalAccessAllMediaPermission)
                 && qnResourceAccessManager->hasPermission(user, layout, Qn::ViewContentPermission);
        });

    /* Do not warn if there are no such users - no side effects in any case. */
    if (!accessibleToCustomUsers)
        return true;

    QnMessageBox messageBox(
        QnMessageBox::Warning,
        Qn::Empty_Help,
        tr("Save Layout..."),
        tr("Changes will affect many users"),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        mainWindow());
    messageBox.setDefaultButton(QDialogButtonBox::Cancel);
    messageBox.setInformativeText(tr("This layout is shared. By changing this layout you change it for all users who have it."));
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::SharedLayoutEdit;
        qnSettings->setShowOnceMessages(messagesFilter);
    }

    return result == QDialogButtonBox::Ok;
}

bool QnWorkbenchLayoutsHandler::confirmDeleteSharedLayouts(const QnLayoutResourceList& layouts)
{
    /* Check if user have already silenced this warning. */
    if (qnSettings->showOnceMessages().testFlag(Qn::ShowOnceMessage::DeleteSharedLayouts))
        return true;

    /* Checking if custom users have access to this shared layout. */
    auto allUsers = qnResPool->getResources<QnUserResource>();
    auto accessibleToCustomUsers = boost::algorithm::any_of(
        allUsers,
        [layouts](const QnUserResourcePtr& user)
        {
            return !qnResourceAccessManager->hasGlobalPermission(user, Qn::GlobalAccessAllMediaPermission)
                && boost::algorithm::any_of(
                    layouts,
                    [user](const QnLayoutResourcePtr& layout)
                    {
                        return qnResourceAccessManager->hasPermission(user, layout, Qn::ViewContentPermission);
                    });
        });

    /* Do not warn if there are no such users - no side effects in any case. */
    if (!accessibleToCustomUsers)
        return true;

    QnMessageBox messageBox(
        QnMessageBox::Warning,
        Qn::Empty_Help,
        tr("Delete Layouts..."),
        tr("Changes will affect many users"),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        mainWindow());
    messageBox.setDefaultButton(QDialogButtonBox::Cancel);
    messageBox.setInformativeText(tr("These %n layouts are shared. "
        "By deleting these layouts you delete them from all users who have it.", "", layouts.size()));
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));
    messageBox.addCustomWidget(new QnResourceListView(layouts));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::DeleteSharedLayouts;
        qnSettings->setShowOnceMessages(messagesFilter);
    }

    return result == QDialogButtonBox::Ok;
}

bool QnWorkbenchLayoutsHandler::confirmLayoutChangeForUser(const QnUserResourcePtr& user, const LayoutChange& change)
{
    /* Check if user have already silenced warning about kept access. */
    if (qnSettings->showOnceMessages().testFlag(Qn::ShowOnceMessage::UserLayoutItemsRemoved))
        return true;

    /* Calculate removed cameras that are still directly accessible. */

    auto accessible = qnResourceAccessManager->accessibleResources(user);
    QSet<QnUuid> directlyAccessible;
    for (const QnResourcePtr& resource : change.removed)
    {
        QnUuid id = resource->getId();
        if (accessible.contains(id))
            directlyAccessible << id;
    }

    if (directlyAccessible.isEmpty())
        return true;

    auto mediaResources = qnResPool->getResources(directlyAccessible);

    QnMessageBox messageBox(
        QnMessageBox::Warning,
        Qn::Empty_Help,
        tr("Save Layout..."),
        tr("User will keep access to %n removed cameras", "", directlyAccessible.size()),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        mainWindow());
    messageBox.setDefaultButton(QDialogButtonBox::Cancel);
    messageBox.setInformativeText(tr("To remove access go to User Settings."));
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));
    messageBox.addCustomWidget(new QnResourceListView(mediaResources));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::UserLayoutItemsRemoved;
        qnSettings->setShowOnceMessages(messagesFilter);
    }

    return result == QDialogButtonBox::Ok;
}

bool QnWorkbenchLayoutsHandler::confirmDeleteLayoutsForUser(const QnUserResourcePtr& user, const QnLayoutResourceList& layouts)
{
    NX_ASSERT(user);
    if (!user)
        return true;

    if (qnResourceAccessManager->hasGlobalPermission(user, Qn::GlobalAccessAllMediaPermission))
        return true;

    /* Never ask for own layouts. */
    if (user == context()->user())
        return true;

    /* Check if user have already silenced warning about kept access. */
    if (qnSettings->showOnceMessages().testFlag(Qn::ShowOnceMessage::DeleteUserLayouts))
        return true;

    /* Calculate removed cameras that are still directly accessible. */
    QSet<QnResourcePtr> removedResources;
    for (const auto& layout : layouts)
    {
        auto snapshot = snapshotManager()->snapshot(layout);
        removedResources += layoutResources(snapshot.items);
    }

    auto accessible = qnResourceAccessManager->accessibleResources(user);
    QSet<QnUuid> directlyAccessible;
    for (const QnResourcePtr& resource : removedResources)
    {
        QnUuid id = resource->getId();
        if (accessible.contains(id))
            directlyAccessible << id;
    }

    if (directlyAccessible.isEmpty())
        return true;

    auto mediaResources = qnResPool->getResources(directlyAccessible);

    QnMessageBox messageBox(
        QnMessageBox::Warning,
        Qn::Empty_Help,
        tr("Delete Layouts..."),
        tr("User %1 will keep access to %n removed cameras", "", directlyAccessible.size()).arg(user->getName()),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        mainWindow());
    messageBox.setDefaultButton(QDialogButtonBox::Cancel);
    messageBox.setInformativeText(tr("To remove access go to User Settings."));
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));
    messageBox.addCustomWidget(new QnResourceListView(mediaResources));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::DeleteUserLayouts;
        qnSettings->setShowOnceMessages(messagesFilter);
    }

    return result == QDialogButtonBox::Ok;
}

bool QnWorkbenchLayoutsHandler::confirmLayoutChangeForGroup(const QnUuid& groupId, const LayoutChange& change)
{
    auto groupUsers = qnResPool->getResources<QnUserResource>().filtered(
        [groupId]
        (const QnUserResourcePtr& user)
        {
            return user->userGroup() == groupId;
        }
    );

    NX_ASSERT(!groupUsers.isEmpty(), "Invalid user group");
    if (groupUsers.isEmpty())
        return true;

    /* If group contains of 1 user, work as if it was just custom user. */
//     if (groupUsers.size() == 1)
//         return confirmLayoutChangeForUser(groupUsers.first(), layout);

    //TODO: #GDM #implement me
    /* Calculate added cameras, which were not available to group before, and show warning. */
    /* Calculate removed cameras and show another warning. 1 ok, second cancel, so what? */
    return true;
}

bool QnWorkbenchLayoutsHandler::confirmStopSharingLayouts(const QnUserResourcePtr& user, const QnLayoutResourceList& layouts)
{
    /* Check if user have already silenced this warning. */
    if (qnSettings->showOnceMessages().testFlag(Qn::ShowOnceMessage::StopSharingLayoutUser))
        return true;

    QnLayoutResourceList accessible = layouts.filtered(
        [user]
        (const QnLayoutResourcePtr& layout)
        {
            return qnResourceAccessManager->hasPermission(user, layout, Qn::ViewContentPermission);
        });
    NX_ASSERT(accessible.size() == layouts.size(), "We are not supposed to stop sharing inaccessible layouts.");

    if (qnResourceAccessManager->hasGlobalPermission(user, Qn::GlobalAccessAllMediaPermission))
        return true;

    /* Calculate all resources that were available through these layouts. */
    QSet<QnResourcePtr> mediaResources;
    for (const auto& layout: accessible)
        mediaResources += layoutResources(layout->getItems());

    /* Skip resources that still will be accessible. */
    for (const auto& directlyAvailable: qnResPool->getResources(qnResourceAccessManager->accessibleResources(user)))
        mediaResources -= directlyAvailable;

    /* Skip resources that still will be accessible through other dialogs. */
    for (const auto& layout : qnResPool->getResources<QnLayoutResource>().filtered(
        [user, layouts]
        (const QnLayoutResourcePtr& layout)
        {
            return layout->isShared()
                && qnResourceAccessManager->hasPermission(user, layout, Qn::ViewContentPermission)
                && !layouts.contains(layout);
        }
        ))
    {
        mediaResources -= layoutResources(layout->getItems());
    }

    if (mediaResources.isEmpty())
        return true;

    QnMessageBox messageBox(
        QnMessageBox::Warning,
        Qn::Empty_Help,
        tr("Stop Sharing Layout..."),
        tr("By deleting shared layout from user you remove access to cameras on it"),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        mainWindow());
    messageBox.setDefaultButton(QDialogButtonBox::Cancel);
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));
    messageBox.addCustomWidget(new QnResourceListView(mediaResources.toList()));
    messageBox.setInformativeText(tr(
        "User will keep access to cameras, which he has on other shared layouts, or which are "
        "assigned to him directly. Access will be lost to the following %n cameras:",
        "",
        mediaResources.size()));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::StopSharingLayoutUser;
        qnSettings->setShowOnceMessages(messagesFilter);
    }

    return result == QDialogButtonBox::Ok;
}

void QnWorkbenchLayoutsHandler::grantAccessRightsForUser(const QnUserResourcePtr& user, const LayoutChange& change)
{
    if (qnResourceAccessManager->hasGlobalPermission(user, Qn::GlobalAccessAllMediaPermission))
        return;

    auto inaccessible = [user](const QnResourcePtr& resource)
    {
        if (resource->hasFlags(Qn::media) || resource->hasFlags(Qn::web_page))
            return !qnResourceAccessManager->hasPermission(user, resource, Qn::ViewContentPermission);

        /* Silently ignoring servers. */
        return false;
    };

    auto accessible = qnResourceAccessManager->accessibleResources(user);
    for (const auto& toShare : change.added.filtered(inaccessible))
        accessible << toShare->getId();
    qnResourcesChangesManager->saveAccessibleResources(user->getId(), accessible);
}

QDialogButtonBox::StandardButton QnWorkbenchLayoutsHandler::askOverrideLayout(QDialogButtonBox::StandardButtons buttons,
    QDialogButtonBox::StandardButton defaultButton)
{
    return QnMessageBox::warning(
        mainWindow(),
        tr("Layout already exists."),
        tr("A layout with the same name already exists. Would you like to overwrite it?"),
        buttons,
        defaultButton
    );
}

bool QnWorkbenchLayoutsHandler::canRemoveLayouts(const QnLayoutResourceList &layouts)
{
    for (const QnLayoutResourcePtr &layout : layouts)
    {
        if (!accessController()->hasPermissions(layout, Qn::RemovePermission))
            return false;
    }
    return true;
}

void QnWorkbenchLayoutsHandler::removeLayouts(const QnLayoutResourceList &layouts)
{
    if (layouts.isEmpty())
        return;

    if (!canRemoveLayouts(layouts))
        return;

    QnLayoutResourceList remoteResources;
    for (const QnLayoutResourcePtr &layout : layouts)
    {
        NX_ASSERT(!layout->isFile());
        if (layout->isFile())
            continue;

        if (layout->hasFlags(Qn::local))
            qnResPool->removeResource(layout); /*< This one can be simply deleted from resource pool. */
        else
            remoteResources << layout;
    }

    qnResourcesChangesManager->deleteResources(remoteResources);
}

bool QnWorkbenchLayoutsHandler::closeLayouts(const QnWorkbenchLayoutList &layouts, bool waitForReply, bool force)
{
    QnLayoutResourceList resources;
    foreach(QnWorkbenchLayout *layout, layouts)
        resources.push_back(layout->resource());

    return closeLayouts(resources, waitForReply, force);
}

bool QnWorkbenchLayoutsHandler::closeLayouts(const QnLayoutResourceList &resources, bool waitForReply, bool force)
{
    QN_SCOPED_VALUE_ROLLBACK(&m_closingLayouts, true);

    if (resources.empty())
        return true;

    QnLayoutResourceList saveableResources, rollbackResources;
    if (!force)
    {
        for (const QnLayoutResourcePtr &resource : resources)
        {
            bool changed = snapshotManager()->flags(resource).testFlag(Qn::ResourceIsChanged);
            if (!changed)
                continue;

            bool saveable = accessController()->hasPermissions(resource, Qn::SavePermission);
            if (saveable)
                saveableResources.push_back(resource);
            else
                rollbackResources.push_back(resource);
        }
    }


    rollbackResources.append(saveableResources);
    saveableResources.clear();
    closeLayouts(resources, rollbackResources, saveableResources);
    return true;
}

void QnWorkbenchLayoutsHandler::closeLayouts(const QnLayoutResourceList &resources, const QnLayoutResourceList &rollbackResources, const QnLayoutResourceList &saveResources)
{
    if (!saveResources.empty())
    {
        QnLayoutResourceList fileResources, normalResources, videowallReviewResources;
        foreach(const QnLayoutResourcePtr &resource, saveResources)
        {
            if (resource->isFile())
            {
                fileResources.push_back(resource);
            }
            else if (!resource->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull())
            {
                videowallReviewResources.push_back(resource);
            }
            else
            {
                normalResources.push_back(resource);
            }
        }

        QnCounter *counter = new QnCounter(0, this);
        connect(counter, SIGNAL(reachedZero()), counter, SLOT(deleteLater()));

        if (!normalResources.isEmpty())
        {
            auto callback = [counter](bool success, const QnLayoutResourcePtr &layout)
            {
                QN_UNUSED(success, layout);
                counter->decrement();
            };
            for (const QnLayoutResourcePtr& layout : normalResources)
                if (snapshotManager()->save(layout, callback))
                    counter->increment();
        }

        QnWorkbenchExportHandler *exportHandler = context()->instance<QnWorkbenchExportHandler>();
        foreach(const QnLayoutResourcePtr &fileResource, fileResources)
        {
            bool isReadOnly = !(accessController()->permissions(fileResource) & Qn::WritePermission);

            if (exportHandler->saveLocalLayout(fileResource, isReadOnly, false, counter, SLOT(decrement())))
                counter->increment();
        }

        if (!videowallReviewResources.isEmpty())
        {
            foreach(const QnLayoutResourcePtr &layout, videowallReviewResources)
            {
                //TODO: #GDM #VW #LOW refactor common code to common place
                if (!context()->instance<QnWorkbenchVideoWallHandler>()->saveReviewLayout(layout, [this, layout, counter](int reqId, ec2::ErrorCode errorCode)
                {
                    Q_UNUSED(reqId);
                    snapshotManager()->setFlags(layout, snapshotManager()->flags(layout) & ~Qn::ResourceIsBeingSaved);
                    counter->decrement();
                    if (errorCode != ec2::ErrorCode::ok)
                        return;
                    snapshotManager()->setFlags(layout, snapshotManager()->flags(layout) & ~Qn::ResourceIsChanged);
                }))
                    continue;
                snapshotManager()->setFlags(layout, snapshotManager()->flags(layout) | Qn::ResourceIsBeingSaved);
                counter->increment();
            }
        }

        // magic that will invoke slot if counter is empty and delete it afterwards
        counter->increment();
        counter->decrement();
    }

    for (const QnLayoutResourcePtr &resource: rollbackResources)
        snapshotManager()->restore(resource);

    for (const QnLayoutResourcePtr &resource: resources)
    {
        if (QnWorkbenchLayout *layout = QnWorkbenchLayout::instance(resource))
        {
            workbench()->removeLayout(layout);
            delete layout;
        }

        if (resource->hasFlags(Qn::local) && !resource->isFile())
            qnResPool->removeResource(resource);
    }
}

bool QnWorkbenchLayoutsHandler::closeAllLayouts(bool waitForReply, bool force)
{
    return closeLayouts(qnResPool->getResources<QnLayoutResource>(), waitForReply, force);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnWorkbenchLayoutsHandler::at_newUserLayoutAction_triggered()
{
    QnUserResourcePtr user = menu()->currentParameters(sender()).resource().dynamicCast<QnUserResource>();
    if (!user)
        user = context()->user();

    if (!user)
        return;

    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, mainWindow()));
    dialog->setWindowTitle(tr("New Layout"));
    dialog->setText(tr("Enter the name of the layout to create:"));
    dialog->setName(generateUniqueLayoutName(user, tr("New Layout"), tr("New Layout %1")));
    dialog->setWindowModality(Qt::ApplicationModal);

    QDialogButtonBox::StandardButton button;
    do
    {
        if (!dialog->exec())
            return;

        button = QDialogButtonBox::Yes;
        QnLayoutResourceList existing = alreadyExistingLayouts(dialog->name(), user->getId());

        if (!canRemoveLayouts(existing))
        {
            QnMessageBox::warning(
                mainWindow(),
                tr("Layout already exists."),
                tr("A layout with the same name already exists. You do not have the rights to overwrite it.")
            );
            return;
        }

        if (!existing.isEmpty())
        {
            bool allAreLocal = true;
            for (const QnLayoutResourcePtr &layout : existing)
                allAreLocal &= layout->hasFlags(Qn::local);
            if (allAreLocal)
            {
                removeLayouts(existing);
                break;
            }

            button = askOverrideLayout(QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel, QDialogButtonBox::Yes);
            if (button == QDialogButtonBox::Cancel)
                return;
            if (button == QDialogButtonBox::Yes)
            {
                removeLayouts(existing);
            }
        }
    } while (button != QDialogButtonBox::Yes);

    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setId(QnUuid::createUuid());
    layout->setName(dialog->name());
    layout->setParentId(user->getId());
    qnResPool->addResource(layout);

    snapshotManager()->save(layout, [this](bool success, const QnLayoutResourcePtr &layout) { at_layout_saved(success, layout); });

    menu()->trigger(QnActions::OpenSingleLayoutAction, QnActionParameters(layout));
}

void QnWorkbenchLayoutsHandler::at_saveLayoutAction_triggered()
{
    saveLayout(menu()->currentParameters(sender()).resource().dynamicCast<QnLayoutResource>());
}

void QnWorkbenchLayoutsHandler::at_saveCurrentLayoutAction_triggered()
{
    saveLayout(workbench()->currentLayout()->resource());
}

void QnWorkbenchLayoutsHandler::at_saveLayoutForCurrentUserAsAction_triggered()
{
    saveLayoutAs(
        menu()->currentParameters(sender()).resource().dynamicCast<QnLayoutResource>(),
        context()->user()
    );
}

void QnWorkbenchLayoutsHandler::at_saveLayoutAsAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());

    saveLayoutAs(
        parameters.resource().dynamicCast<QnLayoutResource>(),
        parameters.argument<QnUserResourcePtr>(Qn::UserResourceRole)
    );
}

void QnWorkbenchLayoutsHandler::at_saveCurrentLayoutAsAction_triggered()
{
    saveLayoutAs(
        workbench()->currentLayout()->resource(),
        context()->user()
    );
}

void QnWorkbenchLayoutsHandler::at_closeLayoutAction_triggered()
{
    closeLayouts(menu()->currentParameters(sender()).layouts());
}

void QnWorkbenchLayoutsHandler::at_closeAllButThisLayoutAction_triggered()
{
    QnWorkbenchLayoutList layouts = menu()->currentParameters(sender()).layouts();
    if (layouts.empty())
        return;

    QnWorkbenchLayoutList layoutsToClose = workbench()->layouts();
    foreach(QnWorkbenchLayout *layout, layouts)
        layoutsToClose.removeOne(layout);

    closeLayouts(layoutsToClose);
}

void QnWorkbenchLayoutsHandler::at_removeFromServerAction_triggered()
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
        if (qnResPool->isAutoGeneratedLayout(layout) || layout->hasFlags(Qn::local))
            canAutoDelete << layout;
        else if (layout->isShared())
            shared << layout;
        else if (owner)
            common[owner] << layout;
    }
    removeLayouts(canAutoDelete);

    if (confirmDeleteSharedLayouts(shared))
        removeLayouts(shared);

    auto users = common.keys();
    std::sort(
        users.begin(), users.end(),
        [](const auto& l, const auto &r)
        {
            return nx::utils::naturalStringLess(l->getName(), r->getName());
        });

    for (const auto &user : users)
    {
        auto userLayouts = common[user];
        NX_ASSERT(!userLayouts.isEmpty());
        if (confirmDeleteLayoutsForUser(user, userLayouts))
            removeLayouts(userLayouts);
    }
}

void QnWorkbenchLayoutsHandler::at_shareLayoutAction_triggered()
{
    auto params = menu()->currentParameters(sender());
    auto layout = params.resource().dynamicCast<QnLayoutResource>();
    auto user = params.argument<QnUserResourcePtr>(Qn::UserResourceRole);

    NX_ASSERT(layout);
    NX_ASSERT(user);
    if (!layout || !user)
        return;

    NX_ASSERT(!layout->isFile());
    if (layout->isFile())
        return;

    if (!layout->isShared())
        layout->setParentId(QnUuid());
    NX_ASSERT(layout->isShared());

    /* If layout is changed, it will automatically be saved here (and become shared if needed).
     * Also we do not grant direct access to cameras anyway as layout will become shared
     * and do not ask confirmation, so we do not use common saveLayout() method anyway. */
    if (!snapshotManager()->save(layout,
        [this]
        (bool success, const QnLayoutResourcePtr &layout)
        {
            QN_UNUSED(success, layout);
        }))
        return;


    /* Admins anyway have all shared layouts. */
    if (qnResourceAccessManager->hasGlobalPermission(user, Qn::GlobalAdminPermission))
        return;

    auto accessible = qnResourceAccessManager->accessibleResources(user->getId());
    if (accessible.contains(layout->getId()))
        return;

    accessible << layout->getId();
    qnResourcesChangesManager->saveAccessibleResources(user->getId(), accessible);
}

void QnWorkbenchLayoutsHandler::at_stopSharingLayoutAction_triggered()
{
    auto params = menu()->currentParameters(sender());
    auto layouts = params.resources().filtered<QnLayoutResource>();
    auto user = params.argument<QnUserResourcePtr>(Qn::UserResourceRole);
    NX_ASSERT(user);
    if (!user)
        return;

    //TODO: #GDM what if we've been disconnected while confirming?
    if (!confirmStopSharingLayouts(user, layouts))
        return;

    //TODO: #GDM #implement stop sharing for roled user

    auto accessible = qnResourceAccessManager->accessibleResources(user->getId());
    for (const auto& layout : layouts)
    {
        NX_ASSERT(!layout->isFile());
        accessible.remove(layout->getId());
    }
    qnResourcesChangesManager->saveAccessibleResources(user->getId(), accessible);
}

void QnWorkbenchLayoutsHandler::at_openNewTabAction_triggered()
{
    QnWorkbenchLayout *layout = new QnWorkbenchLayout(this);

    layout->setName(generateUniqueLayoutName(context()->user(), tr("New Layout"), tr("New Layout %1")));

    workbench()->addLayout(layout);
    workbench()->setCurrentLayout(layout);
}

void QnWorkbenchLayoutsHandler::at_workbench_layoutsChanged()
{
    if (m_closingLayouts)
        return;

    if (!workbench()->layouts().empty())
        return;

    menu()->trigger(QnActions::OpenNewTabAction);
}

void QnWorkbenchLayoutsHandler::at_layout_saved(bool success, const QnLayoutResourcePtr &layout)
{
    if (success)
        return;

    if (!layout->hasFlags(Qn::local) || QnWorkbenchLayout::instance(layout))
        return;

    int button = QnResourceListDialog::exec(
        mainWindow(),
        QnResourceList() << layout,
        tr("Error"),
        tr("Could not save the following %n layout(s) to Server.", "", 1),
        tr("Do you want to restore these %n layout(s)?", "", 1),
        QDialogButtonBox::Yes | QDialogButtonBox::No
    );
    if (button == QDialogButtonBox::Yes)
    {
        workbench()->addLayout(new QnWorkbenchLayout(layout, this));
        workbench()->setCurrentLayout(workbench()->layouts().back());
    }
    else
    {
        qnResPool->removeResource(layout);
    }

}

bool QnWorkbenchLayoutsHandler::tryClose(bool force)
{
    return closeAllLayouts(true, force);
}

void QnWorkbenchLayoutsHandler::forcedUpdate()
{
    //do nothing
}
