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

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/handlers/workbench_export_handler.h>     //TODO: #GDM dependencies
#include <ui/workbench/handlers/workbench_videowall_handler.h>  //TODO: #GDM dependencies
#include <ui/workbench/workbench_state_manager.h>

#include <utils/common/counter.h>
#include <utils/common/string.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_value_rollback.h>

namespace
{
    QString generateUniqueLayoutName(const QnUserResourcePtr &user, const QString &defaultName, const QString &nameTemplate)
    {
        QStringList usedNames;
        QnUuid parentId = user ? user->getId() : QnUuid();
        for (const auto layout : qnResPool->getResources<QnLayoutResource>())
        {
            if (layout->isShared() || layout->getParentId() == parentId)
                usedNames.push_back(layout->getName());
        }

        return generateUniqueString(usedNames, defaultName, nameTemplate);
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
}

QnWorkbenchLayoutsHandler::QnWorkbenchLayoutsHandler(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_workbenchStateDelegate(new QnBasicWorkbenchStateDelegate<QnWorkbenchLayoutsHandler>(this)),
    m_closingLayouts(false)
{
    connect(action(QnActions::NewUserLayoutAction),                &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_newUserLayoutAction_triggered);
    connect(action(QnActions::SaveLayoutAction),                   &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_saveLayoutAction_triggered);
    connect(action(QnActions::SaveLayoutAsAction),                 &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_saveLayoutAsAction_triggered);
    connect(action(QnActions::SaveLayoutForCurrentUserAsAction),   &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_saveLayoutForCurrentUserAsAction_triggered);
    connect(action(QnActions::SaveCurrentLayoutAction),            &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_saveCurrentLayoutAction_triggered);
    connect(action(QnActions::SaveCurrentLayoutAsAction),          &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_saveCurrentLayoutAsAction_triggered);
    connect(action(QnActions::CloseLayoutAction),                  &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_closeLayoutAction_triggered);
    connect(action(QnActions::CloseAllButThisLayoutAction),        &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_closeAllButThisLayoutAction_triggered);
    connect(action(QnActions::ShareLayoutAction),                  &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_shareLayoutAction_triggered);
    connect(action(QnActions::StopSharingLayoutAction),            &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_stopSharingLayoutAction_triggered);
    connect(action(QnActions::OpenNewTabAction),                   &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_openNewTabAction_triggered);

    /* We're using queued connection here as modifying a field in its change notification handler may lead to problems. */
    connect(workbench(),                             &QnWorkbench::layoutsChanged,  this,   &QnWorkbenchLayoutsHandler::at_workbench_layoutsChanged, Qt::QueuedConnection);
}

QnWorkbenchLayoutsHandler::~QnWorkbenchLayoutsHandler()
{}

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
        snapshotManager()->save(layout, [this](bool success, const QnLayoutResourcePtr &layout) { at_layout_saved(success, layout); });
    }
}

void QnWorkbenchLayoutsHandler::saveLayoutAs(const QnLayoutResourcePtr &layout, const QnUserResourcePtr &user)
{
    if (!layout)
        return;

    if (!user)
        return;

    if (layout->isFile())
    {
        context()->instance<QnWorkbenchExportHandler>()->doAskNameAndExportLocalLayout(layout->getLocalRange(), layout, Qn::LayoutLocalSaveAs);
        return;
    }
    else if (!layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull())
    {
        return;
    }

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
                if (snapshotManager()->isLocal(layout))
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
    bool shouldDelete = snapshotManager()->isLocal(layout) &&
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
    if (!canRemoveLayouts(layouts))
        return;

    foreach(const QnLayoutResourcePtr &layout, layouts)
    {
        if (snapshotManager()->isLocal(layout))
            qnResPool->removeResource(layout); /* This one can be simply deleted from resource pool. */
        else
            connection2()->getLayoutManager(Qn::kDefaultUserAccess)->remove( layout->getId(), ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone );
    }
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

    foreach(const QnLayoutResourcePtr &resource, rollbackResources)
        snapshotManager()->restore(resource);

    foreach(const QnLayoutResourcePtr &resource, resources)
    {
        if (QnWorkbenchLayout *layout = QnWorkbenchLayout::instance(resource))
        {
            workbench()->removeLayout(layout);
            delete layout;
        }

        Qn::ResourceSavingFlags flags = snapshotManager()->flags(resource);
        if ((flags & (Qn::ResourceIsLocal | Qn::ResourceIsBeingSaved)) == Qn::ResourceIsLocal) /* Local, not being saved. */
            if (!resource->isFile()) /* Not a file. */
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
            foreach(const QnLayoutResourcePtr &layout, existing)
                allAreLocal &= snapshotManager()->isLocal(layout);
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
    auto accessible = qnResourceAccessManager->accessibleResources(user->getId());
    if (accessible.contains(layout->getId()))
        return;

    if (!layout->isShared())
        layout->setParentId(QnUuid());

    /* If layout is changed, it will automatically be saved here. */
    saveLayout(layout);

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

    if (!snapshotManager()->isLocal(layout) || QnWorkbenchLayout::instance(layout))
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
