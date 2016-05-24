#include "workbench_layouts_handler.h"

#include <api/app_server_connection.h>

#include <client/client_globals.h>
#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>

#include <core/resource_management/resource_pool.h>

#include <nx_ec/dummy_handler.h>
#include <nx_ec/managers/abstract_layout_manager.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/common/ui_resource_name.h>
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

QnWorkbenchLayoutsHandler::QnWorkbenchLayoutsHandler(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_workbenchStateDelegate(new QnBasicWorkbenchStateDelegate<QnWorkbenchLayoutsHandler>(this)),
    m_closingLayouts(false)
{
    connect(action(QnActions::NewUserLayoutAction),                &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_newUserLayoutAction_triggered);
    connect(action(QnActions::NewGlobalLayoutAction),              &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_newGlobalLayoutAction_triggered);
    connect(action(QnActions::SaveLayoutAction),                   &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_saveLayoutAction_triggered);
    connect(action(QnActions::SaveLayoutAsAction),                 &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_saveLayoutAsAction_triggered);
    connect(action(QnActions::SaveLayoutForCurrentUserAsAction),   &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_saveLayoutForCurrentUserAsAction_triggered);
    connect(action(QnActions::SaveCurrentLayoutAction),            &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_saveCurrentLayoutAction_triggered);
    connect(action(QnActions::SaveCurrentLayoutAsAction),          &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_saveCurrentLayoutAsAction_triggered);
    connect(action(QnActions::CloseLayoutAction),                  &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_closeLayoutAction_triggered);
    connect(action(QnActions::CloseAllButThisLayoutAction),        &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_closeAllButThisLayoutAction_triggered);

    /* We're using queued connection here as modifying a field in its change notification handler may lead to problems. */
    connect(workbench(),                             &QnWorkbench::layoutsChanged,  this,   &QnWorkbenchLayoutsHandler::at_workbench_layoutsChanged, Qt::QueuedConnection);
}

QnWorkbenchLayoutsHandler::~QnWorkbenchLayoutsHandler() {
}

ec2::AbstractECConnectionPtr QnWorkbenchLayoutsHandler::connection2() const {
    return QnAppServerConnectionFactory::getConnection2();
}

void QnWorkbenchLayoutsHandler::renameLayout(const QnLayoutResourcePtr &layout, const QString &newName) {
    QnUserResourcePtr user = qnResPool->getResourceById<QnUserResource>(layout->getParentId());

    QnLayoutResourceList existing = alreadyExistingLayouts(newName, user, layout);
    if (!canRemoveLayouts(existing)) {
        QnMessageBox::warning(
            mainWindow(),
            tr("Layout already exists."),
            tr("A layout with the same name already exists. You do not have the rights to overwrite it.")
        );
        return;
    }

    if (!existing.isEmpty()) {
        if (askOverrideLayout(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, QDialogButtonBox::Cancel) == QDialogButtonBox::Cancel)
            return;
        removeLayouts(existing);
    }

    bool changed = snapshotManager()->isChanged(layout);

    layout->setName(newName);

    if(!changed)
        snapshotManager()->save(layout, [this](bool success, const QnLayoutResourcePtr &layout) {at_layout_saved(success, layout); });
}

void QnWorkbenchLayoutsHandler::saveLayout(const QnLayoutResourcePtr &layout) {
    if(!layout)
        return;

    if(!snapshotManager()->isSaveable(layout))
        return;

    if(!(accessController()->permissions(layout) & Qn::SavePermission))
        return;

    if (layout->isFile()) {
        bool isReadOnly = !(accessController()->permissions(layout) & Qn::WritePermission);
        QnWorkbenchExportHandler *exportHandler = context()->instance<QnWorkbenchExportHandler>();
        exportHandler->saveLocalLayout(layout, isReadOnly, true); // overwrite layout file
    } else if (!layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull()) {
        //TODO: #GDM #VW #LOW refactor common code to common place
        if (context()->instance<QnWorkbenchVideoWallHandler>()->saveReviewLayout(layout, [this, layout](int reqId, ec2::ErrorCode errorCode) {
            snapshotManager()->setFlags(layout, snapshotManager()->flags(layout) & ~Qn::ResourceIsBeingSaved);
            at_layout_saved(errorCode == ec2::ErrorCode::ok, layout);
            if (errorCode != ec2::ErrorCode::ok)
                return;
            snapshotManager()->setFlags(layout, snapshotManager()->flags(layout) & ~Qn::ResourceIsChanged);
        }))
            snapshotManager()->setFlags(layout, snapshotManager()->flags(layout) | Qn::ResourceIsBeingSaved);
    } else {
        //TODO: #GDM #Common check existing layouts.
        //TODO: #GDM #Common all remotes layout checking and saving should be done in one place
        snapshotManager()->save(layout, [this](bool success, const QnLayoutResourcePtr &layout) {at_layout_saved(success, layout); });
    }
}

void QnWorkbenchLayoutsHandler::saveLayoutAs(const QnLayoutResourcePtr &layout, const QnUserResourcePtr &user) {
    if(!layout)
        return;

    if(!user)
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
    if(name.isEmpty()) {
        QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Save | QDialogButtonBox::Cancel, mainWindow()));
        dialog->setWindowTitle(tr("Save Layout As"));
        dialog->setText(tr("Enter Layout Name:"));

        QString proposedName = hasSavePermission
            ? layout->getName()
            : generateUniqueLayoutName(context()->user(), layout->getName(), layout->getName() + lit(" %1"));

        dialog->setName(proposedName);
        setHelpTopic(dialog.data(), Qn::SaveLayout_Help);

        QDialogButtonBox::StandardButton button = QDialogButtonBox::Cancel;
        do {
            if (!dialog->exec())
                return;

            /* Check if we were disconnected (server shut down) while the dialog was open. */
            if (!context()->user())
                return;

            if(dialog->clickedButton() != QDialogButtonBox::Save)
                return;

            name = dialog->name();

            // that's the case when user press "Save As" and enters the same name as this layout already has
            if (name == layout->getName() && user == layoutOwnerUser && hasSavePermission) {
                if(snapshotManager()->isLocal(layout)) {
                    saveLayout(layout);
                    return;
                }

                switch (askOverrideLayout(QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel, QDialogButtonBox::Yes)) {
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
            QnLayoutResourceList existing = alreadyExistingLayouts(name, user, excludingSelfLayout);
            if (!canRemoveLayouts(existing)) {
                QnMessageBox::warning(
                    mainWindow(),
                    tr("Layout already exists."),
                    tr("A layout with the same name already exists. You do not have the rights to overwrite it.")
                );
                dialog->setName(proposedName);
                continue;
            }

            button = QDialogButtonBox::Yes;
            if (!existing.isEmpty()) {
                button = askOverrideLayout(QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel, QDialogButtonBox::Yes);
                if (button == QDialogButtonBox::Cancel)
                    return;
                if (button == QDialogButtonBox::Yes) {
                    removeLayouts(existing);
                }
            }
        } while (button != QDialogButtonBox::Yes);
    } else {
        QnLayoutResourceList existing = alreadyExistingLayouts(name, user, layout);
        if (!canRemoveLayouts(existing)) {
            QnMessageBox::warning(
                mainWindow(),
                tr("Layout already exists."),
                tr("A layout with the same name already exists. You do not have the rights to overwrite it.")
            );
            return;
        }

        if (!existing.isEmpty()) {
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
    context()->resourcePool()->addResource(newLayout);

    QnLayoutItemDataList items = layout->getItems().values();
    QHash<QnUuid, QnUuid> newUuidByOldUuid;
    for(int i = 0; i < items.size(); i++) {
        QnUuid newUuid = QnUuid::createUuid();
        newUuidByOldUuid[items[i].uuid] = newUuid;
        items[i].uuid = newUuid;
    }
    for(int i = 0; i < items.size(); i++)
        items[i].zoomTargetUuid = newUuidByOldUuid.value(items[i].zoomTargetUuid, QnUuid());
    newLayout->setItems(items);

    const bool isCurrent = (layout == workbench()->currentLayout()->resource());
    bool shouldDelete = snapshotManager()->isLocal(layout) &&
            (name == layout->getName() || isCurrent);

    /* If it is current layout, close it and open the new one instead. */
    if( isCurrent &&
        user == layoutOwnerUser )   //making current only new layout of current user
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

    snapshotManager()->save(newLayout, [this](bool success, const QnLayoutResourcePtr &layout) {at_layout_saved(success, layout); });
    if (shouldDelete)
        removeLayouts(QnLayoutResourceList() << layout);
}

QnLayoutResourceList QnWorkbenchLayoutsHandler::alreadyExistingLayouts(const QString &name, const QnUserResourcePtr &user, const QnLayoutResourcePtr &layout) {
    QnLayoutResourceList result;
    foreach (const QnLayoutResourcePtr &existingLayout, resourcePool()->getResourcesWithParentId(user->getId()).filtered<QnLayoutResource>()) {
        if (existingLayout == layout)
            continue;
        if (existingLayout->getName().toLower() != name.toLower())
            continue;
        result << existingLayout;
    }
    return result;
}

QDialogButtonBox::StandardButton QnWorkbenchLayoutsHandler::askOverrideLayout(QDialogButtonBox::StandardButtons buttons,
                                                                        QDialogButtonBox::StandardButton defaultButton) {
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
    for (const QnLayoutResourcePtr &layout: layouts)
    {
        if (!accessController()->hasPermissions(layout, Qn::RemovePermission))
            return false;
    }
    return true;
}

void QnWorkbenchLayoutsHandler::removeLayouts(const QnLayoutResourceList &layouts) {
    if (!canRemoveLayouts(layouts))
        return;

    foreach(const QnLayoutResourcePtr &layout, layouts) {
        if(snapshotManager()->isLocal(layout))
            resourcePool()->removeResource(layout); /* This one can be simply deleted from resource pool. */
        else
            connection2()->getLayoutManager()->remove( layout->getId(), ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone );
    }
}

bool QnWorkbenchLayoutsHandler::closeLayouts(const QnWorkbenchLayoutList &layouts, bool waitForReply, bool force) {
    QnLayoutResourceList resources;
    foreach(QnWorkbenchLayout *layout, layouts)
        resources.push_back(layout->resource());

    return closeLayouts(resources, waitForReply, force);
}

bool QnWorkbenchLayoutsHandler::closeLayouts(const QnLayoutResourceList &resources, bool waitForReply, bool force) {
    QN_SCOPED_VALUE_ROLLBACK(&m_closingLayouts, true);

    if(resources.empty())
        return true;

    bool needToAsk = false;
    bool ignoreAll = qnSettings->ignoreUnsavedLayouts();
    QnLayoutResourceList saveableResources, rollbackResources;
    if (!force) {
        foreach(const QnLayoutResourcePtr &resource, resources) {
            Qn::ResourceSavingFlags flags = snapshotManager()->flags(resource);

            bool changed = flags & Qn::ResourceIsChanged;
            bool saveable = accessController()->permissions(resource) & Qn::SavePermission;

            needToAsk |= (changed && saveable);

            if(changed) {
                if(saveable) {
                    saveableResources.push_back(resource);
                } else {
                    rollbackResources.push_back(resource);
                }
            }
        }
    }

    bool closeAll = true;
    bool saveAll = false;
    if(needToAsk && !ignoreAll) {
        QScopedPointer<QnResourceListDialog> dialog(new QnResourceListDialog(mainWindow()));
        dialog->setResources(saveableResources);
        dialog->setWindowTitle(tr("Close Layouts"));
        dialog->setText(tr("The following %n layout(s) are not saved. Do you want to save them?", "", saveableResources.size()));
        dialog->setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel);
        dialog->setReadOnly(false);
        dialog->showIgnoreCheckbox();
        setHelpTopic(dialog.data(), Qn::SaveLayout_Help);
        dialog->exec();
        QDialogButtonBox::StandardButton button = dialog->clickedButton();

        switch (button) {
        case QDialogButtonBox::NoButton:
        case QDialogButtonBox::Cancel:
            closeAll = false;
            saveAll = false;
            break;
        case QDialogButtonBox::No:
            closeAll = true;
            saveAll = false;
            qnSettings->setIgnoreUnsavedLayouts(dialog->isIgnoreCheckboxChecked());
            break;
        default: /* QDialogButtonBox::Yes */
            closeAll = true;
            saveAll = true;
            qnSettings->setIgnoreUnsavedLayouts(dialog->isIgnoreCheckboxChecked());
            break;
        }
    }

    if(closeAll) {
        if(!saveAll) {
            rollbackResources.append(saveableResources);
            saveableResources.clear();
        }

        if(!waitForReply || saveableResources.empty()) {
            closeLayouts(resources, rollbackResources, saveableResources, NULL, NULL);
            return true;
        } else {
            QScopedPointer<QnResourceListDialog> dialog(new QnResourceListDialog(mainWindow()));
            dialog->setWindowTitle(tr("Saving Layouts"));
            dialog->setText(tr("The following %n layout(s) are being saved.", "", saveableResources.size()));
            dialog->setBottomText(tr("Please wait."));
            dialog->setStandardButtons(0);
            dialog->setResources(QnResourceList(saveableResources));

            QScopedPointer<QnMultiEventEater> eventEater(new QnMultiEventEater(Qn::IgnoreEvent));
            eventEater->addEventType(QEvent::KeyPress);
            eventEater->addEventType(QEvent::KeyRelease); /* So that ESC doesn't close the dialog. */
            eventEater->addEventType(QEvent::Close);
            dialog->installEventFilter(eventEater.data());

            closeLayouts(resources, rollbackResources, saveableResources, dialog.data(), SLOT(accept()));
            dialog->exec();

            return true;
        }
    } else {
        return false;
    }
}

void QnWorkbenchLayoutsHandler::closeLayouts(const QnLayoutResourceList &resources, const QnLayoutResourceList &rollbackResources, const QnLayoutResourceList &saveResources, QObject *target, const char *slot) {
    if(!saveResources.empty()) {
        QnLayoutResourceList fileResources, normalResources, videowallReviewResources;
        foreach(const QnLayoutResourcePtr &resource, saveResources) {
            if(resource->isFile())
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
        if (target && slot)
            connect(counter, SIGNAL(reachedZero()), target, slot);
        connect(counter, SIGNAL(reachedZero()), counter, SLOT(deleteLater()));

        if(!normalResources.isEmpty())
        {
            auto callback = [counter](bool success, const QnLayoutResourcePtr &layout)
            {
                QN_UNUSED(success, layout);
                counter->decrement();
            };
            for (const QnLayoutResourcePtr& layout: normalResources)
                if (snapshotManager()->save(layout, callback))
                    counter->increment();
        }

        QnWorkbenchExportHandler *exportHandler = context()->instance<QnWorkbenchExportHandler>();
        foreach(const QnLayoutResourcePtr &fileResource, fileResources) {
            bool isReadOnly = !(accessController()->permissions(fileResource) & Qn::WritePermission);

            if(exportHandler->saveLocalLayout(fileResource, isReadOnly, false, counter, SLOT(decrement())))
                counter->increment();
        }

        if (!videowallReviewResources.isEmpty()) {
            foreach(const QnLayoutResourcePtr &layout, videowallReviewResources) {
                //TODO: #GDM #VW #LOW refactor common code to common place
                if (!context()->instance<QnWorkbenchVideoWallHandler>()->saveReviewLayout(layout, [this, layout, counter](int reqId, ec2::ErrorCode errorCode) {
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

    foreach(const QnLayoutResourcePtr &resource, resources) {
        if(QnWorkbenchLayout *layout = QnWorkbenchLayout::instance(resource)) {
            workbench()->removeLayout(layout);
            delete layout;
        }

        Qn::ResourceSavingFlags flags = snapshotManager()->flags(resource);
        if((flags & (Qn::ResourceIsLocal | Qn::ResourceIsBeingSaved)) == Qn::ResourceIsLocal) /* Local, not being saved. */
            if(!resource->isFile()) /* Not a file. */
                resourcePool()->removeResource(resource);
    }
}

bool QnWorkbenchLayoutsHandler::closeAllLayouts(bool waitForReply, bool force) {
    return closeLayouts(resourcePool()->getResources<QnLayoutResource>(), waitForReply, force);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnWorkbenchLayoutsHandler::at_newUserLayoutAction_triggered()
{
    QnUserResourcePtr user = menu()->currentParameters(sender()).resource().dynamicCast<QnUserResource>();
    if(user.isNull())
        return;

    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, mainWindow()));
    dialog->setWindowTitle(tr("New Layout"));
    dialog->setText(tr("Enter the name of the layout to create:"));
    dialog->setName(generateUniqueLayoutName(user, tr("New Layout"), tr("New Layout %1")));
    dialog->setWindowModality(Qt::ApplicationModal);

    QDialogButtonBox::StandardButton button;
    do {
        if(!dialog->exec())
            return;

        button = QDialogButtonBox::Yes;
        QnLayoutResourceList existing = alreadyExistingLayouts(dialog->name(), user);

        if (!canRemoveLayouts(existing)) {
            QnMessageBox::warning(
                mainWindow(),
                tr("Layout already exists."),
                tr("A layout with the same name already exists. You do not have the rights to overwrite it.")
            );
            return;
        }

        if (!existing.isEmpty()) {
            bool allAreLocal = true;
            foreach (const QnLayoutResourcePtr &layout, existing)
                allAreLocal &= snapshotManager()->isLocal(layout);
            if (allAreLocal) {
                removeLayouts(existing);
                break;
            }

            button = askOverrideLayout(QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel, QDialogButtonBox::Yes);
            if (button == QDialogButtonBox::Cancel)
                return;
            if (button == QDialogButtonBox::Yes) {
                removeLayouts(existing);
            }
        }
    } while (button != QDialogButtonBox::Yes);

    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setId(QnUuid::createUuid());
    layout->setName(dialog->name());
    layout->setParentId(user->getId());
    resourcePool()->addResource(layout);

    snapshotManager()->save(layout, [this](bool success, const QnLayoutResourcePtr &layout) {at_layout_saved(success, layout); });

    menu()->trigger(QnActions::OpenSingleLayoutAction, QnActionParameters(layout));
}

void QnWorkbenchLayoutsHandler::at_newGlobalLayoutAction_triggered()
{
    auto globalLayouts = []
    {
        return qnResPool->getResources().filtered<QnLayoutResource>([](const QnLayoutResourcePtr& layout)
        {
            return layout->isGlobal();
        });
    };


    QStringList usedNames;
    for (const auto &existing: globalLayouts())
        usedNames << existing->getName().toLower();


    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, mainWindow()));
    dialog->setWindowTitle(tr("New Global Layout"));
    dialog->setText(tr("Enter the name of the global layout to create:"));
    dialog->setName(generateUniqueString(usedNames, tr("Global Layout"), tr("Global Layout %1")));
    dialog->setWindowModality(Qt::ApplicationModal);

    QDialogButtonBox::StandardButton button;
    do {
        if (!dialog->exec())
            return;

        button = QDialogButtonBox::Yes;
        const QString name = dialog->name();
        QnLayoutResourceList existing = globalLayouts().filtered([name](const QnLayoutResourcePtr& layout)
        {
            return layout->getName().toLower() == name.toLower();
        });

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

    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setId(QnUuid::createUuid());
    layout->setName(dialog->name());
    resourcePool()->addResource(layout);

    snapshotManager()->save(layout, [this](bool success, const QnLayoutResourcePtr &layout) {at_layout_saved(success, layout); });

    menu()->trigger(QnActions::OpenSingleLayoutAction, QnActionParameters(layout));
}

void QnWorkbenchLayoutsHandler::at_saveLayoutAction_triggered() {
    saveLayout(menu()->currentParameters(sender()).resource().dynamicCast<QnLayoutResource>());
}

void QnWorkbenchLayoutsHandler::at_saveCurrentLayoutAction_triggered() {
    saveLayout(workbench()->currentLayout()->resource());
}

void QnWorkbenchLayoutsHandler::at_saveLayoutForCurrentUserAsAction_triggered() {
    saveLayoutAs(
        menu()->currentParameters(sender()).resource().dynamicCast<QnLayoutResource>(),
        context()->user()
    );
}

void QnWorkbenchLayoutsHandler::at_saveLayoutAsAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    saveLayoutAs(
        parameters.resource().dynamicCast<QnLayoutResource>(),
        parameters.argument<QnUserResourcePtr>(Qn::UserResourceRole)
    );
}

void QnWorkbenchLayoutsHandler::at_saveCurrentLayoutAsAction_triggered() {
    saveLayoutAs(
        workbench()->currentLayout()->resource(),
        context()->user()
    );
}

void QnWorkbenchLayoutsHandler::at_closeLayoutAction_triggered() {
    closeLayouts(menu()->currentParameters(sender()).layouts());
}

void QnWorkbenchLayoutsHandler::at_closeAllButThisLayoutAction_triggered() {
    QnWorkbenchLayoutList layouts = menu()->currentParameters(sender()).layouts();
    if(layouts.empty())
        return;

    QnWorkbenchLayoutList layoutsToClose = workbench()->layouts();
    foreach(QnWorkbenchLayout *layout, layouts)
        layoutsToClose.removeOne(layout);

    closeLayouts(layoutsToClose);
}

void QnWorkbenchLayoutsHandler::at_workbench_layoutsChanged() {
    if (m_closingLayouts)
        return;

    if(!workbench()->layouts().empty())
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
        resourcePool()->removeResource(layout);
    }

}

bool QnWorkbenchLayoutsHandler::tryClose(bool force) {
    return closeAllLayouts(true, force);
}

void QnWorkbenchLayoutsHandler::forcedUpdate() {
    //do nothing
}
