#include "workbench_layouts_handler.h"

#include <api/app_server_connection.h>

#include <client/client_globals.h>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>

#include <core/resource_management/resource_pool.h>

#include <nx_ec/dummy_handler.h>

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
#include <utils/common/event_processors.h>

QnWorkbenchLayoutsHandler::QnWorkbenchLayoutsHandler(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_workbenchStateDelegate(new QnBasicWorkbenchStateDelegate<QnWorkbenchLayoutsHandler>(this))
{
    connect(action(Qn::NewUserLayoutAction),                &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_newUserLayoutAction_triggered);
    connect(action(Qn::SaveLayoutAction),                   &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_saveLayoutAction_triggered);
    connect(action(Qn::SaveLayoutAsAction),                 &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_saveLayoutAsAction_triggered);
    connect(action(Qn::SaveLayoutForCurrentUserAsAction),   &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_saveLayoutForCurrentUserAsAction_triggered);
    connect(action(Qn::SaveCurrentLayoutAction),            &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_saveCurrentLayoutAction_triggered);
    connect(action(Qn::SaveCurrentLayoutAsAction),          &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_saveCurrentLayoutAsAction_triggered);
    connect(action(Qn::CloseLayoutAction),                  &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_closeLayoutAction_triggered);
    connect(action(Qn::CloseAllButThisLayoutAction),        &QAction::triggered,    this,   &QnWorkbenchLayoutsHandler::at_closeAllButThisLayoutAction_triggered);
}

QnWorkbenchLayoutsHandler::~QnWorkbenchLayoutsHandler() {
}

ec2::AbstractECConnectionPtr QnWorkbenchLayoutsHandler::connection2() const {
    return QnAppServerConnectionFactory::getConnection2();
}

void QnWorkbenchLayoutsHandler::renameLayout(const QnLayoutResourcePtr &layout, const QString &newName) {
    QnUserResourcePtr user = qnResPool->getResourceById(layout->getParentId()).dynamicCast<QnUserResource>();

    QnLayoutResourceList existing = alreadyExistingLayouts(newName, user, layout);
    if (!canRemoveLayouts(existing)) {
        QMessageBox::warning(
            mainWindow(),
            tr("Layout already exists"),
            tr("Layout with the same name already exists and you do not have the rights to overwrite it.")
        );
        return;
    }

    if (!existing.isEmpty()) {
        if (askOverrideLayout(QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Cancel)
            return;
        removeLayouts(existing);
    }

    bool changed = snapshotManager()->isChanged(layout);

    layout->setName(newName);

    if(!changed)
        snapshotManager()->save(layout, this, SLOT(at_layouts_saved(int, const QnResourceList &, int)));
}

void QnWorkbenchLayoutsHandler::saveLayout(const QnLayoutResourcePtr &layout) {
    if(!layout)
        return;

    if(!snapshotManager()->isSaveable(layout))
        return;

    if(!(accessController()->permissions(layout) & Qn::SavePermission))
        return;

    if (snapshotManager()->isFile(layout)) {
        bool isReadOnly = !(accessController()->permissions(layout) & Qn::WritePermission);
        QnWorkbenchExportHandler *exportHandler = context()->instance<QnWorkbenchExportHandler>();
        exportHandler->saveLocalLayout(layout, isReadOnly, true); // overwrite layout file
    } else if (!layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull()) {
        //TODO: #GDM #VW #LOW refactor common code to common place
        if (context()->instance<QnWorkbenchVideoWallHandler>()->saveReviewLayout(layout, [this, layout](int reqId, ec2::ErrorCode errorCode) {
            snapshotManager()->setFlags(layout, snapshotManager()->flags(layout) & ~Qn::ResourceIsBeingSaved);
            at_layouts_saved(static_cast<int>(errorCode), QnResourceList() << layout, reqId);           
            if (errorCode != ec2::ErrorCode::ok)
                return;
            snapshotManager()->setFlags(layout, snapshotManager()->flags(layout) & ~Qn::ResourceIsChanged);
        }))
            snapshotManager()->setFlags(layout, snapshotManager()->flags(layout) | Qn::ResourceIsBeingSaved);
    } else {
        //TODO: #GDM #Common check existing layouts.
        //TODO: #GDM #Common all remotes layout checking and saving should be done in one place
        snapshotManager()->save(layout, this, SLOT(at_layouts_saved(int, const QnResourceList &, int)));
    }
}

void QnWorkbenchLayoutsHandler::saveLayoutAs(const QnLayoutResourcePtr &layout, const QnUserResourcePtr &user) {
    if(!layout)
        return;

    if(!user)
        return;

    if(snapshotManager()->isFile(layout)) {
        context()->instance<QnWorkbenchExportHandler>()->doAskNameAndExportLocalLayout(layout->getLocalRange(), layout, Qn::LayoutLocalSaveAs);
        return;
    } else if (!layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull()) {
        return;
    }

    const QnResourcePtr layoutOwnerUser = layout->getParentResource();
    bool hasSavePermission = accessController()->hasPermissions(layout, Qn::SavePermission);

    QString name = menu()->currentParameters(sender()).argument<QString>(Qn::ResourceNameRole).trimmed();
    if(name.isEmpty()) {
        QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Save | QDialogButtonBox::Cancel, mainWindow()));
        dialog->setWindowTitle(tr("Save Layout As"));
        dialog->setText(tr("Enter layout name:"));

        QString proposedName = hasSavePermission
            ? layout->getName()
            : generateUniqueLayoutName(context()->user(), layout->getName(), layout->getName() + lit(" %1"));

        dialog->setName(proposedName);
        setHelpTopic(dialog.data(), Qn::SaveLayout_Help);

        QMessageBox::Button button = QMessageBox::Cancel;
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

                switch (askOverrideLayout(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes)) {
                case QMessageBox::Cancel:
                    return;
                case QMessageBox::Yes:
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
                QMessageBox::warning(
                    mainWindow(),
                    tr("Layout already exists"),
                    tr("Layout with the same name already exists and you do not have the rights to overwrite it.")
                );
                dialog->setName(proposedName);
                continue;
            }

            button = QMessageBox::Yes;
            if (!existing.isEmpty()) {
                button = askOverrideLayout(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
                if (button == QMessageBox::Cancel)
                    return;
                if (button == QMessageBox::Yes) {
                    removeLayouts(existing);
                }
            }
        } while (button != QMessageBox::Yes);
    } else {
        QnLayoutResourceList existing = alreadyExistingLayouts(name, user, layout);
        if (!canRemoveLayouts(existing)) {
            QMessageBox::warning(
                mainWindow(),
                tr("Layout already exists"),
                tr("Layout with the same name already exists and you do not have the rights to overwrite it.")
            );
            return;
        }

        if (!existing.isEmpty()) {
            if (askOverrideLayout(QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Cancel)
                return;
            removeLayouts(existing);
        }
    }

    QnLayoutResourcePtr newLayout;

    newLayout = QnLayoutResourcePtr(new QnLayoutResource());
    newLayout->setId(QUuid::createUuid());
    newLayout->setTypeByName(lit("Layout"));
    newLayout->setName(name);
    newLayout->setParentId(user->getId());
    newLayout->setCellSpacing(layout->cellSpacing());
    newLayout->setCellAspectRatio(layout->cellAspectRatio());
    newLayout->setUserCanEdit(context()->user() == user);
    newLayout->setBackgroundImageFilename(layout->backgroundImageFilename());
    newLayout->setBackgroundOpacity(layout->backgroundOpacity());
    newLayout->setBackgroundSize(layout->backgroundSize());
    context()->resourcePool()->addResource(newLayout);

    QnLayoutItemDataList items = layout->getItems().values();
    QHash<QUuid, QUuid> newUuidByOldUuid;
    for(int i = 0; i < items.size(); i++) {
        QUuid newUuid = QUuid::createUuid();
        newUuidByOldUuid[items[i].uuid] = newUuid;
        items[i].uuid = newUuid;
    }
    for(int i = 0; i < items.size(); i++)
        items[i].zoomTargetUuid = newUuidByOldUuid.value(items[i].zoomTargetUuid, QUuid());
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

    snapshotManager()->save(newLayout, this, SLOT(at_layouts_saved(int, const QnResourceList &, int)));
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

QMessageBox::StandardButton QnWorkbenchLayoutsHandler::askOverrideLayout(QMessageBox::StandardButtons buttons,
                                                                        QMessageBox::StandardButton defaultButton) {
    return QMessageBox::warning(
        mainWindow(),
        tr("Layout already exists"),
        tr("Layout with the same name already exists. Do you want to overwrite it?"),
        buttons,
        defaultButton
    );
}

bool QnWorkbenchLayoutsHandler::canRemoveLayouts(const QnLayoutResourceList &layouts) {
    foreach(const QnLayoutResourcePtr &layout, layouts) {
        if (!(accessController()->permissions(layout) & Qn::RemovePermission))
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
    if(resources.empty())
        return true;

    bool needToAsk = false;
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
    if(needToAsk) {
        QDialogButtonBox::StandardButton button = QnResourceListDialog::exec(
            mainWindow(),
            QnResourceList(saveableResources),
            tr("Close Layouts"),
            tr("The following %n layout(s) are not saved. Do you want to save them?", "", saveableResources.size()),
            QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel,
            false
        );

        switch (button) {
        case QDialogButtonBox::NoButton:
        case QDialogButtonBox::Cancel:
            closeAll = false;
            saveAll = false;
            break;
        case QDialogButtonBox::No:
            closeAll = true;
            saveAll = false;
            break;
        default:
            closeAll = true;
            saveAll = true;
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
            if(snapshotManager()->isFile(resource)) {
                fileResources.push_back(resource);
            } else if (!resource->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull()) {
                videowallReviewResources.push_back(resource);
            } else {
                normalResources.push_back(resource);
            }
        }

        QnCounter *counter = new QnCounter(0, this);
        if (target && slot)
            connect(counter, SIGNAL(reachedZero()), target, slot);
        connect(counter, SIGNAL(reachedZero()), counter, SLOT(deleteLater()));

        if(!normalResources.isEmpty()) {
            counter->increment();
            snapshotManager()->save(normalResources, counter, SLOT(decrement()));
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
            if(!snapshotManager()->isFile(resource)) /* Not a file. */
                resourcePool()->removeResource(resource);
    }
}

bool QnWorkbenchLayoutsHandler::closeAllLayouts(bool waitForReply, bool force) {
    return closeLayouts(resourcePool()->getResources<QnLayoutResource>(), waitForReply, force);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnWorkbenchLayoutsHandler::at_newUserLayoutAction_triggered() {
    QnUserResourcePtr user = menu()->currentParameters(sender()).resource().dynamicCast<QnUserResource>();
    if(user.isNull())
        return;

    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, mainWindow()));
    dialog->setWindowTitle(tr("New Layout"));
    dialog->setText(tr("Enter the name of the layout to create:"));
    dialog->setName(generateUniqueLayoutName(user, tr("New layout"), tr("New layout %1")));
    dialog->setWindowModality(Qt::ApplicationModal);

    QMessageBox::Button button;
    do {
        if(!dialog->exec())
            return;

        button = QMessageBox::Yes;
        QnLayoutResourceList existing = alreadyExistingLayouts(dialog->name(), user);

        if (!canRemoveLayouts(existing)) {
            QMessageBox::warning(
                mainWindow(),
                tr("Layout already exists"),
                tr("Layout with the same name already exists and you do not have the rights to overwrite it.")
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

            button = askOverrideLayout(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
            if (button == QMessageBox::Cancel)
                return;
            if (button == QMessageBox::Yes) {
                removeLayouts(existing);
            }
        }
    } while (button != QMessageBox::Yes);

    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setId(QUuid::createUuid());
    layout->setTypeByName(lit("Layout"));
    layout->setName(dialog->name());
    layout->setParentId(user->getId());
    layout->setUserCanEdit(context()->user() == user);
    resourcePool()->addResource(layout);

    snapshotManager()->save(layout, this, SLOT(at_layouts_saved(int, const QnResourceList &, int)));

    menu()->trigger(Qn::OpenSingleLayoutAction, QnActionParameters(layout));
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

void QnWorkbenchLayoutsHandler::at_layouts_saved(int status, const QnResourceList &resources, int handle) {
    Q_UNUSED(handle)
    if (status == 0)
        return;

    QnLayoutResourceList layoutResources = resources.filtered<QnLayoutResource>();
    QnLayoutResourceList reopeningLayoutResources;
    foreach(const QnLayoutResourcePtr &layoutResource, layoutResources)
        if(snapshotManager()->isLocal(layoutResource) && !QnWorkbenchLayout::instance(layoutResource))
            reopeningLayoutResources.push_back(layoutResource);

    if(!reopeningLayoutResources.empty()) {
        int button = QnResourceListDialog::exec(
            mainWindow(),
            resources,
            tr("Error"),
            tr("Could not save the following %n layout(s) to Server.", "", reopeningLayoutResources.size()),
            tr("Do you want to restore these %n layout(s)?", "", reopeningLayoutResources.size()),
            QDialogButtonBox::Yes | QDialogButtonBox::No
        );
        if(button == QMessageBox::Yes) {
            foreach(const QnLayoutResourcePtr &layoutResource, layoutResources)
                workbench()->addLayout(new QnWorkbenchLayout(layoutResource, this));
            workbench()->setCurrentLayout(workbench()->layouts().back());
        } else {
            foreach(const QnLayoutResourcePtr &layoutResource, layoutResources)
                resourcePool()->removeResource(layoutResource);
        }
    }
}

bool QnWorkbenchLayoutsHandler::tryClose(bool force) {
    return closeAllLayouts(true, force);
}
