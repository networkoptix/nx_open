#include "workbench_action_handler.h"

#include <cassert>

#include <QtCore/QProcess>
#include <QtCore/QSettings>

#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>
#include <QtGui/QImage>

#include <utils/common/environment.h>
#include <utils/common/delete_later.h>
#include <utils/common/mime_data.h>
#include <utils/common/event_processors.h>

#include <core/resourcemanagment/resource_discovery_manager.h>
#include <core/resourcemanagment/resource_pool.h>

#include <api/SessionManager.h>

#include <camera/resource_display.h>
#include <camera/camdisplay.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_target_types.h>
#include <ui/actions/action_target_provider.h>

#include <ui/dialogs/about_dialog.h>
#include <ui/dialogs/login_dialog.h>
#include <ui/dialogs/tags_edit_dialog.h>
#include <ui/dialogs/server_settings_dialog.h>
#include <ui/dialogs/connection_testing_dialog.h>
#include <ui/dialogs/camera_settings_dialog.h>
#include <ui/dialogs/layout_name_dialog.h>
#include <ui/dialogs/user_settings_dialog.h>
#include <ui/dialogs/resource_list_dialog.h>
#include <ui/preferences/preferencesdialog.h>
#include <youtube/youtubeuploaddialog.h>

#include <ui/graphics/items/resource_widget.h>

#include "eventmanager.h"
#include "file_processor.h"

#include "workbench.h"
#include "workbench_synchronizer.h"
#include "workbench_layout.h"
#include "workbench_item.h"
#include "workbench_context.h"
#include "workbench_layout_snapshot_manager.h"
#include "workbench_resource.h"
#include "workbench_access_controller.h"

// -------------------------------------------------------------------------- //
// QnResourceStatusReplyProcessor
// -------------------------------------------------------------------------- //
detail::QnResourceStatusReplyProcessor::QnResourceStatusReplyProcessor(QnWorkbenchActionHandler *handler, const QnResourceList &resources, const QList<int> &oldStatuses):
    m_handler(handler),
    m_resources(resources),
    m_oldStatuses(oldStatuses)
{
    assert(oldStatuses.size() == resources.size());
}

void detail::QnResourceStatusReplyProcessor::at_replyReceived(int status, const QByteArray& data, const QByteArray& errorString, int handle) {
    if(m_handler)
        m_handler.data()->at_resources_statusSaved(status, errorString, m_resources, m_oldStatuses);
    
    deleteLater();
}


// -------------------------------------------------------------------------- //
// QnResourceReplyProcessor
// -------------------------------------------------------------------------- //
detail::QnResourceReplyProcessor::QnResourceReplyProcessor(QObject *parent):
    QObject(parent),
    m_handle(0),
    m_status(0)
{}

void detail::QnResourceReplyProcessor::at_replyReceived(int status, const QByteArray& errorString, const QnResourceList &resources, int handle) {
    m_status = status;
    m_errorString = errorString;
    m_resources = resources;
    m_handle = handle;

    emit finished(status, errorString, resources, handle);
}


// -------------------------------------------------------------------------- //
// QnWorkbenchActionHandler
// -------------------------------------------------------------------------- //
QnWorkbenchActionHandler::QnWorkbenchActionHandler(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_selectionUpdatePending(false),
    m_selectionScope(Qn::SceneScope)
{
    connect(context(),                                      SIGNAL(aboutToBeDestroyed()),                   this, SLOT(at_context_aboutToBeDestroyed()));
    connect(context(),                                      SIGNAL(userChanged(const QnUserResourcePtr &)), this, SLOT(at_context_userChanged(const QnUserResourcePtr &)), Qt::QueuedConnection);
    connect(context(),                                      SIGNAL(userChanged(const QnUserResourcePtr &)), this, SLOT(submitDelayedDrops()), Qt::QueuedConnection);
    connect(context(),                                      SIGNAL(userChanged(const QnUserResourcePtr &)), this, SLOT(updateCameraSettingsEditibility()));

    /* We're using queued connection here as modifying a field in its change notification handler may lead to problems. */
    connect(workbench(),                                    SIGNAL(layoutsChanged()), this, SLOT(at_workbench_layoutsChanged()), Qt::QueuedConnection);

    connect(action(Qn::LightMainMenuAction),                SIGNAL(triggered()),    this,   SLOT(at_mainMenuAction_triggered()));
    connect(action(Qn::DarkMainMenuAction),                 SIGNAL(triggered()),    this,   SLOT(at_mainMenuAction_triggered()));
    connect(action(Qn::AboutAction),                        SIGNAL(triggered()),    this,   SLOT(at_aboutAction_triggered()));
    connect(action(Qn::SystemSettingsAction),               SIGNAL(triggered()),    this,   SLOT(at_systemSettingsAction_triggered()));
    connect(action(Qn::OpenFileAction),                     SIGNAL(triggered()),    this,   SLOT(at_openFileAction_triggered()));
    connect(action(Qn::OpenFolderAction),                   SIGNAL(triggered()),    this,   SLOT(at_openFolderAction_triggered()));
    connect(action(Qn::ConnectionSettingsAction),           SIGNAL(triggered()),    this,   SLOT(at_connectionSettingsAction_triggered()));
    connect(action(Qn::ReconnectAction),                    SIGNAL(triggered()),    this,   SLOT(at_reconnectAction_triggered()));
    connect(action(Qn::NextLayoutAction),                   SIGNAL(triggered()),    this,   SLOT(at_nextLayoutAction_triggered()));
    connect(action(Qn::PreviousLayoutAction),               SIGNAL(triggered()),    this,   SLOT(at_previousLayoutAction_triggered()));
    connect(action(Qn::OpenInLayoutAction),                 SIGNAL(triggered()),    this,   SLOT(at_openInLayoutAction_triggered()));
    connect(action(Qn::OpenInCurrentLayoutAction),          SIGNAL(triggered()),    this,   SLOT(at_openInCurrentLayoutAction_triggered()));
    connect(action(Qn::OpenInNewLayoutAction),              SIGNAL(triggered()),    this,   SLOT(at_openInNewLayoutAction_triggered()));
    connect(action(Qn::OpenInNewWindowAction),              SIGNAL(triggered()),    this,   SLOT(at_openInNewWindowAction_triggered()));
    connect(action(Qn::OpenSingleLayoutAction),             SIGNAL(triggered()),    this,   SLOT(at_openLayoutsAction_triggered()));
    connect(action(Qn::OpenMultipleLayoutsAction),          SIGNAL(triggered()),    this,   SLOT(at_openLayoutsAction_triggered()));
    connect(action(Qn::OpenAnyNumberOfLayoutsAction),       SIGNAL(triggered()),    this,   SLOT(at_openLayoutsAction_triggered()));
    connect(action(Qn::OpenNewTabAction),                   SIGNAL(triggered()),    this,   SLOT(at_openNewLayoutAction_triggered()));
    connect(action(Qn::OpenNewWindowAction),                SIGNAL(triggered()),    this,   SLOT(at_openNewWindowAction_triggered()));
    connect(action(Qn::SaveLayoutAction),                   SIGNAL(triggered()),    this,   SLOT(at_saveLayoutAction_triggered()));
    connect(action(Qn::SaveLayoutAsAction),                 SIGNAL(triggered()),    this,   SLOT(at_saveLayoutAsAction_triggered()));
    connect(action(Qn::SaveLayoutForCurrentUserAsAction),   SIGNAL(triggered()),    this,   SLOT(at_saveLayoutForCurrentUserAsAction_triggered()));
    connect(action(Qn::SaveCurrentLayoutAction),            SIGNAL(triggered()),    this,   SLOT(at_saveCurrentLayoutAction_triggered()));
    connect(action(Qn::SaveCurrentLayoutAsAction),          SIGNAL(triggered()),    this,   SLOT(at_saveCurrentLayoutAsAction_triggered()));
    connect(action(Qn::CloseLayoutAction),                  SIGNAL(triggered()),    this,   SLOT(at_closeLayoutAction_triggered()));
    connect(action(Qn::CloseAllButThisLayoutAction),        SIGNAL(triggered()),    this,   SLOT(at_closeAllButThisLayoutAction_triggered()));
    connect(action(Qn::UserSettingsAction),                 SIGNAL(triggered()),    this,   SLOT(at_userSettingsAction_triggered()));
    connect(action(Qn::CameraSettingsAction),               SIGNAL(triggered()),    this,   SLOT(at_cameraSettingsAction_triggered()));
    connect(action(Qn::OpenInCameraSettingsDialogAction),   SIGNAL(triggered()),    this,   SLOT(at_cameraSettingsAction_triggered()));
    connect(action(Qn::SelectionChangeAction),              SIGNAL(triggered()),    this,   SLOT(at_selectionChangeAction_triggered()));
    connect(action(Qn::ServerSettingsAction),               SIGNAL(triggered()),    this,   SLOT(at_serverSettingsAction_triggered()));
    connect(action(Qn::YouTubeUploadAction),                SIGNAL(triggered()),    this,   SLOT(at_youtubeUploadAction_triggered()));
    connect(action(Qn::EditTagsAction),                     SIGNAL(triggered()),    this,   SLOT(at_editTagsAction_triggered()));
    connect(action(Qn::OpenInFolderAction),                 SIGNAL(triggered()),    this,   SLOT(at_openInFolderAction_triggered()));
    connect(action(Qn::DeleteFromDiskAction),               SIGNAL(triggered()),    this,   SLOT(at_deleteFromDiskAction_triggered()));
    connect(action(Qn::RemoveLayoutItemAction),             SIGNAL(triggered()),    this,   SLOT(at_removeLayoutItemAction_triggered()));
    connect(action(Qn::RemoveFromServerAction),             SIGNAL(triggered()),    this,   SLOT(at_removeFromServerAction_triggered()));
    connect(action(Qn::NewUserAction),                      SIGNAL(triggered()),    this,   SLOT(at_newUserAction_triggered()));
    connect(action(Qn::NewUserLayoutAction),                SIGNAL(triggered()),    this,   SLOT(at_newUserLayoutAction_triggered()));
    connect(action(Qn::RenameAction),                       SIGNAL(triggered()),    this,   SLOT(at_renameAction_triggered()));
    connect(action(Qn::DropResourcesAction),                SIGNAL(triggered()),    this,   SLOT(at_dropResourcesAction_triggered()));
    connect(action(Qn::DelayedDropResourcesAction),         SIGNAL(triggered()),    this,   SLOT(at_delayedDropResourcesAction_triggered()));
    connect(action(Qn::DropResourcesIntoNewLayoutAction),   SIGNAL(triggered()),    this,   SLOT(at_dropResourcesIntoNewLayoutAction_triggered()));
    connect(action(Qn::MoveCameraAction),                   SIGNAL(triggered()),    this,   SLOT(at_moveCameraAction_triggered()));
    connect(action(Qn::TakeScreenshotAction),               SIGNAL(triggered()),    this,   SLOT(at_takeScreenshotAction_triggered()));
    connect(action(Qn::ExitAction),                         SIGNAL(triggered()),    this,   SLOT(at_exitAction_triggered()));
}

QnWorkbenchActionHandler::~QnWorkbenchActionHandler() {
    disconnect(context(), NULL, this, NULL);
    disconnect(workbench(), NULL, this, NULL);

    foreach(QAction *action, menu()->actions())
        disconnect(action, NULL, this, NULL);

    /* Clean up. */
    if(m_mainMenu)
        delete m_mainMenu.data();

    if(cameraSettingsDialog())
        delete cameraSettingsDialog();
}

QnAppServerConnectionPtr QnWorkbenchActionHandler::connection() const {
    return QnAppServerConnectionFactory::createConnection();
}

QWidget *QnWorkbenchActionHandler::widget() const {
    return m_widget.data();
}

QString QnWorkbenchActionHandler::newLayoutName() const {
    const QString zeroName = tr("New layout");
    const QString nonZeroName = tr("New layout %1");
    QRegExp pattern = QRegExp(tr("New layout ?([0-9]+)?"));

    QStringList layoutNames;
    QnId parentId = context()->user() ? context()->user()->getId() : QnId();
    foreach(const QnResourcePtr &resource, context()->resourcePool()->getResourcesWithParentId(parentId))
        if(resource->flags() & QnResource::layout)
            layoutNames.push_back(resource->getName());

    /* Prepare name for new layout. */
    int layoutNumber = -1;
    foreach(const QString &name, layoutNames) {
        if(!pattern.exactMatch(name))
            continue;

        layoutNumber = qMax(layoutNumber, pattern.cap(1).toInt());
    }
    layoutNumber++;

    return layoutNumber == 0 ? zeroName : nonZeroName.arg(layoutNumber);
}

bool QnWorkbenchActionHandler::canAutoDelete(const QnResourcePtr &resource) const {
    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    if(!layoutResource)
        return false;
    
    return snapshotManager()->flags(layoutResource) == Qn::LayoutIsLocal; /* Local, not changed and not being saved. */
}

void QnWorkbenchActionHandler::addToLayout(const QnLayoutResourcePtr &layout, const QnResourcePtr &resource, bool usePosition, const QPointF &position) const {
    QnLayoutItemData data;
    data.resource.id = resource->getId();
    data.resource.path = resource->getUniqueId();
    data.uuid = QUuid::createUuid();
    data.flags = QnWorkbenchItem::PendingGeometryAdjustment;
    if(usePosition) {
        data.combinedGeometry = QRectF(position, position); /* Desired position is encoded into a valid rect. */
    } else {
        data.combinedGeometry = QRectF(QPointF(0.5, 0.5), QPointF(-0.5, -0.5)); /* The fact that any position is OK is encoded into an invalid rect. */
    }
    
    layout->addItem(data);
}

void QnWorkbenchActionHandler::addToLayout(const QnLayoutResourcePtr &layout, const QnResourceList &resources, bool usePosition, const QPointF &position) const {
    foreach(const QnResourcePtr &resource, resources)
        addToLayout(layout, resource, usePosition, position);
}

void QnWorkbenchActionHandler::addToLayout(const QnLayoutResourcePtr &layout, const QnMediaResourceList &resources, bool usePosition, const QPointF &position) const {
    foreach(const QnMediaResourcePtr &resource, resources)
        addToLayout(layout, resource, usePosition, position);
}

void QnWorkbenchActionHandler::addToLayout(const QnLayoutResourcePtr &layout, const QList<QString> &files, bool usePosition, const QPointF &position) const {
    addToLayout(layout, addToResourcePool(files), usePosition, position);
}

QnResourceList QnWorkbenchActionHandler::addToResourcePool(const QList<QString> &files) const {
    return QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(files));
}

QnResourceList QnWorkbenchActionHandler::addToResourcePool(const QString &file) const {
    return QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(file));
}

bool QnWorkbenchActionHandler::closeLayouts(const QnWorkbenchLayoutList &layouts, bool waitForReply) {
    QnLayoutResourceList resources;
    foreach(QnWorkbenchLayout *layout, layouts)
        resources.push_back(layout->resource());

    return closeLayouts(resources, waitForReply);
}

bool QnWorkbenchActionHandler::closeLayouts(const QnLayoutResourceList &resources, bool waitForReply) {
    if(resources.empty())
        return true;

    bool needToAsk = false;
    QnLayoutResourceList saveableResources, rollbackResources;
    foreach(const QnLayoutResourcePtr &resource, resources) {
        bool changed, saveable, askable;

        Qn::LayoutFlags flags = snapshotManager()->flags(resource);
        askable = flags == (Qn::LayoutIsChanged | Qn::LayoutIsLocal); /* Changed, local, not being saved. */
        changed = flags & Qn::LayoutIsChanged;
        saveable = accessController()->permissions(resource) & Qn::SavePermission;

        if(askable && saveable)
            needToAsk = true;

        if(changed) {
            if(saveable) {
                saveableResources.push_back(resource);
            } else {
                rollbackResources.push_back(resource);
            }
        }
    }

    bool closeAll = true;
    bool saveAll = false;
    if(needToAsk) {
        QDialogButtonBox::StandardButton button = QnResourceListDialog::exec(
            widget(),
            QnResourceList(saveableResources),
            tr("Close Layouts"),
            tr("The following %n layout(s) are not saved. Do you want to save them?", NULL, saveableResources.size()),
            QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel
        );

        if(button == QDialogButtonBox::Cancel) {
            closeAll = false;
            saveAll = false;
        } else if(button == QDialogButtonBox::No) {
            closeAll = true;
            saveAll = false;
        } else {
            closeAll = true;
            saveAll = true;
        }
    }

    if(closeAll) {
        if(!saveAll) {
            rollbackResources.append(saveableResources);
            saveableResources.clear();
        }

        if(!waitForReply || saveableResources.empty()) {
            closeLayouts(resources, rollbackResources, saveableResources, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
            return true;
        } else {
            QScopedPointer<QnResourceListDialog> dialog(new QnResourceListDialog(widget()));
            dialog->setWindowTitle(tr("Saving Layouts"));
            dialog->setText(tr("The following %n layout(s) are being saved.", NULL, saveableResources.size()));
            dialog->setBottomText(tr("Please wait."));
            dialog->setStandardButtons(0);
            dialog->setResources(QnResourceList(saveableResources));

            QScopedPointer<QnMultiEventEater> eventEater(new QnMultiEventEater(Qn::IgnoreEvent));
            eventEater->addEventType(QEvent::KeyPress);
            eventEater->addEventType(QEvent::KeyRelease); /* So that ESC doesn't close the dialog. */
            eventEater->addEventType(QEvent::Close);
            dialog->installEventFilter(eventEater.data());
            
            QScopedPointer<detail::QnResourceReplyProcessor> processor(new detail::QnResourceReplyProcessor(this));
            connect(processor.data(), SIGNAL(finished(int, const QByteArray &, const QnResourceList &, int)), dialog.data(), SLOT(accept()));

            closeLayouts(resources, rollbackResources, saveableResources, processor.data(), SLOT(at_replyReceived(int, const QByteArray &, const QnResourceList &, int)));
            dialog->exec();

            QnWorkbenchLayoutList currentLayouts = workbench()->layouts();
            at_resources_saved(processor->status(), processor->errorString(), processor->resources(), processor->handle());
            return workbench()->layouts() == currentLayouts; // TODO: This is an ugly hack, think of a better solution.
        }
    } else {
        return false;
    }
}

void QnWorkbenchActionHandler::closeLayouts(const QnLayoutResourceList &resources, const QnLayoutResourceList &rollbackResources, const QnLayoutResourceList &saveResources, QObject *object, const char *slot) {
    if(!saveResources.empty())
        snapshotManager()->save(saveResources, object, slot);

    foreach(const QnLayoutResourcePtr &resource, rollbackResources)
        snapshotManager()->restore(resource);

    foreach(const QnLayoutResourcePtr &resource, resources) {
        if(QnWorkbenchLayout *layout = QnWorkbenchLayout::instance(resource)) {
            workbench()->removeLayout(layout);
            delete layout;
        }

        Qn::LayoutFlags flags = snapshotManager()->flags(resource);
        if((flags & (Qn::LayoutIsLocal | Qn::LayoutIsBeingSaved)) == Qn::LayoutIsLocal) /* Local, not being saved. */
            resourcePool()->removeResource(resource);
    }
}

void QnWorkbenchActionHandler::openNewWindow(const QStringList &args) {
    QStringList arguments = args;
    arguments << QLatin1String("--no-single-application");
    arguments << QLatin1String("--auth");
    arguments << qnSettings->lastUsedConnection().url.toEncoded();

    /* For now, simply open it at another screen. Don't account for 3+ monitor setups. */
    if(widget()) {
        int screen = qApp->desktop()->screenNumber(widget());
        screen = (screen + 1) % qApp->desktop()->screenCount();

        arguments << QLatin1String("--screen");
        arguments << QString::number(screen);
    }

    QProcess::startDetached(qApp->applicationFilePath(), arguments);
}

void QnWorkbenchActionHandler::saveCameraSettingsFromDialog() {
    if(!cameraSettingsDialog())
        return;

    if(!cameraSettingsDialog()->widget()->hasChanges())
        return;

    QnVirtualCameraResourceList cameras = cameraSettingsDialog()->widget()->cameras();
    if(cameras.empty())
        return;

    /* Limit the number of active cameras. */
    int activeCameras = resourcePool()->activeCameras() + cameraSettingsDialog()->widget()->activeCameraCount();
    foreach (const QnVirtualCameraResourcePtr &camera, cameras)
        if (!camera->isScheduleDisabled())
            activeCameras--;

    if (activeCameras > qnLicensePool->getLicenses().totalCameras()) {
        QString message = tr("Licenses limit exceeded (%1 of %2 used). Your schedule will be saved, but will not take effect.").arg(activeCameras).arg(qnLicensePool->getLicenses().totalCameras());
        QMessageBox::warning(widget(), tr("Could not Enable Recording"), message);
        cameraSettingsDialog()->widget()->setCamerasActive(false);
    }

    /* Submit and save it. */
    cameraSettingsDialog()->widget()->submitToResources();
    connection()->saveAsync(cameras, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
}

void QnWorkbenchActionHandler::updateCameraSettingsEditibility() {
    if(!cameraSettingsDialog())
        return;

    Qn::Permissions permissions = accessController()->permissions(cameraSettingsDialog()->widget()->cameras());
    cameraSettingsDialog()->widget()->setReadOnly(!(permissions & Qn::WritePermission));
}

void QnWorkbenchActionHandler::updateCameraSettingsFromSelection() {
    if(!cameraSettingsDialog() || cameraSettingsDialog()->isHidden() || !m_selectionUpdatePending)
        return;

    m_selectionUpdatePending = false;

    QnActionTargetProvider *provider = menu()->targetProvider();
    if(!provider)
        return;

    Qn::ActionScope scope = provider->currentScope();
    if(scope != Qn::SceneScope && scope != Qn::TreeScope) {
        scope = m_selectionScope;
    } else {
        m_selectionScope = scope;
    }

    menu()->trigger(Qn::OpenInCameraSettingsDialogAction, provider->currentTarget(scope));
}

void QnWorkbenchActionHandler::submitDelayedDrops() {
    if(!context()->user())
        return;

    foreach(const QnMimeData &data, m_delayedDrops) {
        QMimeData mimeData;
        data.toMimeData(&mimeData);

        QnResourceList resources = QnWorkbenchResource::deserializeResources(&mimeData);
        menu()->trigger(Qn::OpenInCurrentLayoutAction, resources);
    }

    m_delayedDrops.clear();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchActionHandler::at_context_userChanged(const QnUserResourcePtr &user) {
    if(!user)
        return;

    menu()->trigger(Qn::OpenAnyNumberOfLayoutsAction, QnResourceList(user->getLayouts()));

    /* Delete empty orphaned layouts, move non-empty to the new user. */
    foreach(const QnResourcePtr &resource, context()->resourcePool()->getResourcesWithParentId(QnId())) {
        if(QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>()) {
            if(snapshotManager()->isLocal(layout)) {
                if(layout->getItems().empty()) {
                    resourcePool()->removeResource(layout);
                } else {
                    user->addLayout(layout);
                }
            }
        }
    }
}

void QnWorkbenchActionHandler::at_workbench_layoutsChanged() {
    if(!workbench()->layouts().empty())
        return;

    menu()->trigger(Qn::OpenNewTabAction);
}

void QnWorkbenchActionHandler::at_mainMenuAction_triggered() {
    m_mainMenu = menu()->newMenu(Qn::MainScope);

    action(Qn::LightMainMenuAction)->setMenu(m_mainMenu.data());
    action(Qn::DarkMainMenuAction)->setMenu(m_mainMenu.data());
}

void QnWorkbenchActionHandler::at_nextLayoutAction_triggered() {
    workbench()->setCurrentLayoutIndex((workbench()->currentLayoutIndex() + 1) % workbench()->layouts().size());
}

void QnWorkbenchActionHandler::at_previousLayoutAction_triggered() {
    workbench()->setCurrentLayoutIndex((workbench()->currentLayoutIndex() - 1 + workbench()->layouts().size()) % workbench()->layouts().size());
}

void QnWorkbenchActionHandler::at_openInLayoutAction_triggered() {
    QnLayoutResourcePtr layout = menu()->currentParameter(sender(), Qn::LayoutParameter).value<QnLayoutResourcePtr>();
    if(!layout) {
        qnWarning("No layout provided.");
        return;
    }

    QPointF position = menu()->currentParameter(sender(), Qn::GridPositionParameter).toPointF();

    QnResourceWidgetList widgets = menu()->currentWidgetsTarget(sender());
    if(!widgets.empty() && position.isNull() && layout->getItems().empty()) {
        foreach(const QnResourceWidget *widget, widgets)
            layout->addItem(widget->item()->data());
        return;
    }

    QnMediaResourceList resources = QnResourceCriterion::filter<QnMediaResource>(menu()->currentResourcesTarget(sender()));
    if(!resources.isEmpty()) {
        addToLayout(layout, resources, !position.isNull(), position);
        return;
    }
}

void QnWorkbenchActionHandler::at_openInCurrentLayoutAction_triggered() {
    QVariantMap params = menu()->currentParameters(sender());
    params[Qn::LayoutParameter] = QVariant::fromValue(workbench()->currentLayout()->resource());

    menu()->trigger(Qn::OpenInLayoutAction, menu()->currentTarget(sender()), params);
};

void QnWorkbenchActionHandler::at_openInNewLayoutAction_triggered() {
    menu()->trigger(Qn::OpenNewTabAction);
    menu()->trigger(Qn::OpenInCurrentLayoutAction, menu()->currentTarget(sender()), menu()->currentParameters(sender()));
}

void QnWorkbenchActionHandler::at_openInNewWindowAction_triggered() {
    QnResourceList medias = QnResourceCriterion::filter<QnMediaResource, QnResourceList>(menu()->currentResourcesTarget(sender()));
    if(medias.isEmpty()) 
        return;
    
    QMimeData mimeData;
    QnWorkbenchResource::serializeResources(medias, QnWorkbenchResource::resourceMimeTypes(), &mimeData);
    QnMimeData data(&mimeData);
    QByteArray serializedData;
    QDataStream stream(&serializedData, QIODevice::WriteOnly);
    stream << data;

    QStringList arguments;
    arguments << QLatin1String("--delayed-drop");
    arguments << QLatin1String(serializedData.toBase64().data());

    openNewWindow(arguments);
}

void QnWorkbenchActionHandler::at_openLayoutsAction_triggered() {
    foreach(const QnResourcePtr &resource, menu()->currentResourcesTarget(sender())) {
        QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();

        QnWorkbenchLayout *layout = QnWorkbenchLayout::instance(layoutResource);
        if(layout == NULL) {
            layout = new QnWorkbenchLayout(layoutResource, workbench());
            workbench()->addLayout(layout);
        }
        
        workbench()->setCurrentLayout(layout);
    }
}

void QnWorkbenchActionHandler::at_openNewLayoutAction_triggered() {
    QnWorkbenchLayout *layout = new QnWorkbenchLayout(this);
    layout->setName(newLayoutName());

    workbench()->addLayout(layout);
    workbench()->setCurrentLayout(layout);
}

void QnWorkbenchActionHandler::at_openNewWindowAction_triggered() {
    openNewWindow(QStringList());
}

void QnWorkbenchActionHandler::at_saveLayoutAction_triggered(const QnLayoutResourcePtr &layout) {
    if(!layout)
        return;

    if(!snapshotManager()->isSaveable(layout))
        return;

    if(!(accessController()->permissions(layout) & Qn::SavePermission))
        return;

    snapshotManager()->save(layout, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
}

void QnWorkbenchActionHandler::at_saveLayoutAction_triggered() {
    at_saveLayoutAction_triggered(menu()->currentResourceTarget(sender()).dynamicCast<QnLayoutResource>());
}

void QnWorkbenchActionHandler::at_saveCurrentLayoutAction_triggered() {
    at_saveLayoutAction_triggered(workbench()->currentLayout()->resource());
}

void QnWorkbenchActionHandler::at_saveLayoutAsAction_triggered(const QnLayoutResourcePtr &layout, const QnUserResourcePtr &user) {
    if(!layout)
        return;

    if(!user)
        return;

    QString name = menu()->currentParameter(sender(), Qn::NameParameter).toString();
    if(name.isEmpty()) {
        QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Save | QDialogButtonBox::Cancel, widget()));
        dialog->setWindowTitle(tr("Save Layout As"));
        dialog->setText(tr("Enter layout name:"));
        dialog->setName(layout->getName());
        dialog->exec();
        if(dialog->clickedButton() != QDialogButtonBox::Save)
            return;
        name = dialog->name();
    }

    QnLayoutResourcePtr newLayout;
    if(snapshotManager()->isLocal(layout) && !snapshotManager()->isBeingSaved(layout)) {
        /* Local layout that is not being saved should not be copied. */
        newLayout = layout;
        newLayout->setName(name);
    } else {
        newLayout = QnLayoutResourcePtr(new QnLayoutResource());
        newLayout->setGuid(QUuid::createUuid());
        newLayout->setName(name);

        context()->resourcePool()->addResource(newLayout);
        if(user)
            user->addLayout(newLayout);

        QnLayoutItemDataList items = layout->getItems().values();
        for(int i = 0; i < items.size(); i++)
            items[i].uuid = QUuid::createUuid();
        newLayout->setItems(items);
    }

    snapshotManager()->save(newLayout, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
}

void QnWorkbenchActionHandler::at_saveLayoutForCurrentUserAsAction_triggered() {
    at_saveLayoutAsAction_triggered(
        menu()->currentResourceTarget(sender()).dynamicCast<QnLayoutResource>(), 
        context()->user()
    );
}

void QnWorkbenchActionHandler::at_saveLayoutAsAction_triggered() {
    at_saveLayoutAsAction_triggered(
        menu()->currentResourceTarget(sender()).dynamicCast<QnLayoutResource>(), 
        menu()->currentParameter(sender(), Qn::UserParameter).value<QnUserResourcePtr>()
    );
}

void QnWorkbenchActionHandler::at_saveCurrentLayoutAsAction_triggered() {
    at_saveLayoutAsAction_triggered(
        workbench()->currentLayout()->resource(),
        context()->user()
    );
}

void QnWorkbenchActionHandler::at_closeLayoutAction_triggered() {
    closeLayouts(menu()->currentLayoutsTarget(sender()));
}

void QnWorkbenchActionHandler::at_closeAllButThisLayoutAction_triggered() {
    QnWorkbenchLayoutList layouts = menu()->currentLayoutsTarget(sender());
    if(layouts.empty())
        return;
    
    QnWorkbenchLayoutList layoutsToClose = workbench()->layouts();
    foreach(QnWorkbenchLayout *layout, layouts)
        layoutsToClose.removeOne(layout);

    closeLayouts(layoutsToClose);
}

void QnWorkbenchActionHandler::at_moveCameraAction_triggered() {
    QnResourceList resources = menu()->currentResourcesTarget(sender());
    QnVideoServerResourcePtr server = menu()->currentParameter(sender(), Qn::ServerParameter).value<QnVideoServerResourcePtr>();
    if(!server)
        return;

    QnResourceList modifiedResources;
    QList<int> oldStatuses;

    foreach(const QnResourcePtr &resource, resources) {
        if(resource->getParentId() == server->getId())
            continue; /* Moving resource into its owner does nothing. */

        QnNetworkResourcePtr network = resource.dynamicCast<QnNetworkResource>();
        if(!network)
            continue;

        QnMacAddress mac = network->getMAC();

        QnNetworkResourcePtr replacedNetwork;
        foreach(const QnResourcePtr otherResource, resourcePool()->getResourcesWithParentId(server->getId())) {
            if(QnNetworkResourcePtr otherNetwork = otherResource.dynamicCast<QnNetworkResource>()) {
                if(otherNetwork->getMAC() == mac) {
                    replacedNetwork = otherNetwork;
                    break;
                }
            }
        }

        if(replacedNetwork) {
            oldStatuses.push_back(replacedNetwork->getStatus());
            modifiedResources.push_back(replacedNetwork);
            replacedNetwork->setStatus(QnResource::Offline);

            oldStatuses.push_back(network->getStatus());
            modifiedResources.push_back(network);
            network->setStatus(QnResource::Disabled);
        }
    }

    if(!modifiedResources.empty()) {
        detail::QnResourceStatusReplyProcessor *processor = new detail::QnResourceStatusReplyProcessor(this, modifiedResources, oldStatuses);
        connection()->setResourcesStatusAsync(modifiedResources, processor, SLOT(at_replyReceived(int, const QByteArray &, const QByteArray &, int)));
    }
}

void QnWorkbenchActionHandler::at_dropResourcesAction_triggered() {
    QnResourceList layouts = QnResourceCriterion::filter<QnLayoutResource, QnResourceList>(menu()->currentResourcesTarget(sender()));
    if(!layouts.empty()) {
        menu()->trigger(Qn::OpenAnyNumberOfLayoutsAction, layouts);
    } else {
        /* No layouts? Just open dropped media. */
        menu()->trigger(Qn::OpenInCurrentLayoutAction, menu()->currentTarget(sender()), menu()->currentParameters(sender()));
    }
}

void QnWorkbenchActionHandler::at_dropResourcesIntoNewLayoutAction_triggered() {
    QnResourceList layouts = QnResourceCriterion::filter<QnLayoutResource, QnResourceList>(menu()->currentResourcesTarget(sender()));
    if(layouts.empty()) /* That's media drop, open new layout. */
        menu()->trigger(Qn::OpenNewTabAction);

    menu()->trigger(Qn::DropResourcesAction, menu()->currentTarget(sender()), menu()->currentParameters(sender()));
}

void QnWorkbenchActionHandler::at_delayedDropResourcesAction_triggered() {
    QByteArray data = menu()->currentParameter(sender(), Qn::SerializedResourcesParameter).toByteArray();
    QDataStream stream(&data, QIODevice::ReadOnly);
    QnMimeData mimeData;
    stream >> mimeData;
    if(stream.status() != QDataStream::Ok || mimeData.formats().empty())
        return;

    m_delayedDrops.push_back(mimeData);

    submitDelayedDrops();
}

void QnWorkbenchActionHandler::at_openFileAction_triggered() {
    QScopedPointer<QFileDialog> dialog(new QFileDialog(widget(), tr("Open file")));
    dialog->setOption(QFileDialog::DontUseNativeDialog, true);
    dialog->setFileMode(QFileDialog::ExistingFiles);
    QStringList filters;
    filters << tr("All Supported (*.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp *.jpg *.png *.gif *.bmp *.tiff)");
    filters << tr("Video (*.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp)");
    filters << tr("Pictures (*.jpg *.png *.gif *.bmp *.tiff)");
    filters << tr("All files (*.*)");
    dialog->setNameFilters(filters);
    
    if(dialog->exec())
        addToLayout(workbench()->currentLayout()->resource(), dialog->selectedFiles(), false);
}

void QnWorkbenchActionHandler::at_openFolderAction_triggered() {
    QScopedPointer<QFileDialog> dialog(new QFileDialog(widget(), tr("Open file")));
    dialog->setOption(QFileDialog::DontUseNativeDialog, true);
    dialog->setFileMode(QFileDialog::Directory);
    dialog->setOptions(QFileDialog::ShowDirsOnly);
    
    if(dialog->exec())
        addToLayout(workbench()->currentLayout()->resource(), dialog->selectedFiles(), false);
}

void QnWorkbenchActionHandler::at_aboutAction_triggered() {
    QScopedPointer<AboutDialog> dialog(new AboutDialog(widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_systemSettingsAction_triggered() {
    QScopedPointer<PreferencesDialog> dialog(new PreferencesDialog(context(), widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_connectionSettingsAction_triggered() {
    const QUrl lastUsedUrl = qnSettings->lastUsedConnection().url;
    if (lastUsedUrl.isValid() && lastUsedUrl != QnAppServerConnectionFactory::defaultUrl())
        return;

    QScopedPointer<LoginDialog> dialog(new LoginDialog(context(), widget()));
    dialog->setModal(true);
    if (!dialog->exec())
        return;

    menu()->trigger(Qn::ReconnectAction);
}

void QnWorkbenchActionHandler::at_reconnectAction_triggered() {
    const QnSettings::ConnectionData connection = qnSettings->lastUsedConnection();
    if (!connection.url.isValid()) 
        return;
    
    QnEventManager::instance()->stop();
    SessionManager::instance()->stop();

    QnAppServerConnectionFactory::setDefaultUrl(connection.url);

    // repopulate the resource pool
    QnResource::stopCommandProc();
    QnResourceDiscoveryManager::instance().stop();

    // don't remove local resources
    const QnResourceList remoteResources = resourcePool()->getResourcesWithFlag(QnResource::remote);
    resourcePool()->removeResources(remoteResources);

    qnLicensePool->reset();

    SessionManager::instance()->start();
    QnEventManager::instance()->run();

    QnResourceDiscoveryManager::instance().start();
    QnResource::startCommandProc();
}

void QnWorkbenchActionHandler::at_editTagsAction_triggered() {
    QnResourcePtr resource = menu()->currentResourceTarget(sender());
    if(resource.isNull())
        return;

    QScopedPointer<TagsEditDialog> dialog(new TagsEditDialog(QStringList() << resource->getUniqueId(), widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_cameraSettingsAction_triggered() {
    QnResourceList resources = menu()->currentResourcesTarget(sender());

    if(!cameraSettingsDialog()) {
        m_cameraSettingsDialog = new QnCameraSettingsDialog(widget());
        
        connect(cameraSettingsDialog(), SIGNAL(buttonClicked(QDialogButtonBox::StandardButton)), this, SLOT(at_cameraSettingsDialog_buttonClicked(QDialogButtonBox::StandardButton)));
    }

    if(cameraSettingsDialog()->widget()->resources() != resources) {
        if(cameraSettingsDialog()->isVisible() && cameraSettingsDialog()->widget()->hasChanges()) {
            QDialogButtonBox::StandardButton button = QnResourceListDialog::exec(
                widget(), 
                QnResourceList(cameraSettingsDialog()->widget()->resources()),
                tr("Cameras Not Saved"), 
                tr("Save changes to the following %n camera(s)?", NULL, cameraSettingsDialog()->widget()->resources().size()),
                QDialogButtonBox::Yes | QDialogButtonBox::No
            );
            if(button == QDialogButtonBox::Yes)
                saveCameraSettingsFromDialog();
        }

        cameraSettingsDialog()->widget()->setResources(resources);
        updateCameraSettingsEditibility();
    }

    cameraSettingsDialog()->show();
}

void QnWorkbenchActionHandler::at_cameraSettingsDialog_buttonClicked(QDialogButtonBox::StandardButton button) {
    switch(button) {
    case QDialogButtonBox::Ok:
    case QDialogButtonBox::Apply:
        saveCameraSettingsFromDialog();
        break;
    case QDialogButtonBox::Cancel:
        cameraSettingsDialog()->widget()->updateFromResources();
        break;
    default:
        break;
    }
}

void QnWorkbenchActionHandler::at_selectionChangeAction_triggered() {
    if(!cameraSettingsDialog() || cameraSettingsDialog()->isHidden() || m_selectionUpdatePending)
        return;

    m_selectionUpdatePending = true;
    QTimer::singleShot(50, this, SLOT(updateCameraSettingsFromSelection()));
}

void QnWorkbenchActionHandler::at_serverSettingsAction_triggered() {
    QnResourcePtr resource = menu()->currentResourceTarget(sender());
    if(resource.isNull())
        return;

    QScopedPointer<ServerSettingsDialog> dialog(new ServerSettingsDialog(resource.dynamicCast<QnVideoServerResource>(), widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_youtubeUploadAction_triggered() {
    QnResourcePtr resource = menu()->currentResourceTarget(sender());
    if(resource.isNull())
        return;

    QScopedPointer<YouTubeUploadDialog> dialog(new YouTubeUploadDialog(context(), resource, widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_openInFolderAction_triggered() {
    QnResourcePtr resource = menu()->currentResourceTarget(sender());
    if(resource.isNull())
        return;

    QnEnvironment::showInGraphicalShell(widget(), resource->getUrl());
}

void QnWorkbenchActionHandler::at_deleteFromDiskAction_triggered() {
    QSet<QnResourcePtr> resources = menu()->currentResourcesTarget(sender()).toSet();

    QDialogButtonBox::StandardButton button = QnResourceListDialog::exec(
        widget(), 
        resources.toList(),
        tr("Delete Files"), 
        tr("Are you sure you want to permanently delete these %n file(s)?", 0, resources.size()), 
        QDialogButtonBox::Yes | QDialogButtonBox::Cancel
    );
    if(button != QDialogButtonBox::Yes)
        return;
    
    QnFileProcessor::deleteLocalResources(resources.toList());
}

void QnWorkbenchActionHandler::at_removeLayoutItemAction_triggered() {
    QnLayoutItemIndexList items = menu()->currentLayoutItemsTarget(sender());

    if(items.size() > 1) {
        QDialogButtonBox::StandardButton button = QnResourceListDialog::exec(
            widget(),
            QnActionTargetTypes::resources(items),
            tr("Remove Items"),
            tr("Are you sure you want to remove these %n item(s) from layout?", 0, items.size()),
            QDialogButtonBox::Yes | QDialogButtonBox::No
        );
        if(button != QMessageBox::Yes)
            return;
    }

    QList<QUuid> orphanedUuids;
    foreach(const QnLayoutItemIndex &index, items) {
        if(index.layout()) {
            index.layout()->removeItem(index.uuid());
        } else {
            orphanedUuids.push_back(index.uuid());
        }
    }

    /* If appserver is not running, we may get removal requests without layout resource. */
    if(!orphanedUuids.isEmpty()) {
        QList<QnWorkbenchLayout *> layouts;
        layouts.push_front(workbench()->currentLayout());
        foreach(const QUuid &uuid, orphanedUuids) {
            foreach(QnWorkbenchLayout *layout, layouts) {
                if(QnWorkbenchItem *item = layout->item(uuid)) {
                    qnDeleteLater(item);
                    break;
                }
            }
        }
    }
}

void QnWorkbenchActionHandler::at_renameAction_triggered() {
    QnResourcePtr resource = menu()->currentResourceTarget(sender());
    if(!resource)
        return;

    QString name = menu()->currentParameter(sender(), Qn::NameParameter).toString();
    if(name.isEmpty()) {
        QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, widget()));
        dialog->setWindowTitle(tr("Rename"));
        dialog->setText(tr("Enter new name for the selected item:"));
        dialog->setName(resource->getName());
        dialog->setWindowModality(Qt::ApplicationModal);
        if(!dialog->exec())
            return;
        name = dialog->name();
    }

    if(name == resource->getName())
        return;

    if(QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>()) {
        bool changed = snapshotManager()->isChanged(layout);
        resource->setName(name);
        if(!changed)
            snapshotManager()->save(layout, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
    } else {
        resource->setName(name);
        connection()->saveAsync(resource, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
    }
}

void QnWorkbenchActionHandler::at_removeFromServerAction_triggered() {
    QnResourceList resources = menu()->currentResourcesTarget(sender());
    
    /* User cannot delete himself. */
    resources.removeOne(context()->user());
    if(resources.isEmpty())
        return;

    /* Check if it's OK to delete everything without asking. */
    bool okToDelete = true;
    foreach(const QnResourcePtr &resource, resources) {
        if(!canAutoDelete(resource)) {
            okToDelete = false;
            break;
        }
    }

    /* Ask if needed. */
    if(!okToDelete) {
        QDialogButtonBox::StandardButton button = QnResourceListDialog::exec(
            widget(), 
            resources, 
            tr("Delete Resources"), 
            tr("Do you really want to delete the following %n item(s)?", NULL, resources.size()), 
            QDialogButtonBox::Yes | QDialogButtonBox::Cancel
        );
        okToDelete = button == QMessageBox::Yes;
    }

    if(!okToDelete)
        return; /* User does not want it deleted. */

    foreach(const QnResourcePtr &resource, resources) {
        if(QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>())
            if(snapshotManager()->isLocal(layout))
                resourcePool()->removeResource(resource); /* This one can be simply deleted from resource pool. */

        connection()->deleteAsync(resource, this, SLOT(at_resource_deleted(int, const QByteArray &, const QByteArray &, int)));
    }
}

void QnWorkbenchActionHandler::at_newUserAction_triggered() {
    QnUserResourcePtr user(new QnUserResource());

    QScopedPointer<QnUserSettingsDialog> dialog(new QnUserSettingsDialog(context(), widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setUser(user);
    dialog->setElementFlags(QnUserSettingsDialog::CurrentPassword, 0);
    if(!dialog->exec())
        return;

    dialog->submitToResource();
    user->setGuid(QUuid::createUuid());

    connection()->saveAsync(user, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
}

void QnWorkbenchActionHandler::at_newUserLayoutAction_triggered() {
    QnUserResourcePtr user = menu()->currentResourceTarget(sender()).dynamicCast<QnUserResource>();
    if(user.isNull())
        return;

    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, widget()));
    dialog->setWindowTitle(tr("New Layout"));
    dialog->setText(tr("Enter the name of the layout to create:"));
    dialog->setName(newLayoutName());
    dialog->setWindowModality(Qt::ApplicationModal);
    if(!dialog->exec())
        return;

    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setGuid(QUuid::createUuid());
    layout->setName(dialog->name());
    resourcePool()->addResource(layout);

    user->addLayout(layout);

    snapshotManager()->save(layout, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));

    menu()->trigger(Qn::OpenSingleLayoutAction, layout);
}

void QnWorkbenchActionHandler::at_exitAction_triggered() {
    if(cameraSettingsDialog() && cameraSettingsDialog()->isVisible())
        menu()->trigger(Qn::OpenInCameraSettingsDialogAction, QnResourceList());

    if(closeLayouts(QnResourceCriterion::filter<QnLayoutResource>(resourcePool()->getResources()), true))
        qApp->closeAllWindows();
}

void QnWorkbenchActionHandler::at_takeScreenshotAction_triggered() {
    QnResourceWidgetList widgets = menu()->currentWidgetsTarget(sender());
    if(widgets.size() != 1)
        return;
    QnResourceWidget *widget = widgets[0];
    QnResourceDisplay *display = widget->display();
    const QnVideoResourceLayout *layout = display->videoLayout();

    QImage screenshot;
    if (layout->numberOfChannels() > 0) {
        QList<QImage> images;
        for (int i = 0; i < layout->numberOfChannels(); ++i)
            images.push_back(display->camDisplay()->getScreenshot(i));
        QSize size = images[0].size();
        screenshot = QImage(size.width() * layout->width(), size.height() * layout->height(), QImage::Format_ARGB32);
        screenshot.fill(0);
        
        QPainter p(&screenshot);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        for (int i = 0; i < layout->numberOfChannels(); ++i)
            p.drawImage(QPoint(layout->h_position(i) * size.width(), layout->v_position(i) * size.height()), images[i]);
    } else {
        qnWarning("No channels in resource '%1' of type '%2'.", widget->resource()->getName(), widget->resource()->metaObject()->className());
        return;
    }

    QString suggetion = tr("screenshot");

    QSettings settings;
    settings.beginGroup(QLatin1String("screenshots"));

    QString previousDir = settings.value(QLatin1String("previousDir")).toString();
    QString selectedFilter;
    QString filePath = QFileDialog::getSaveFileName(
        this->widget(), 
        tr("Save Video Screenshot As..."),
        previousDir + QDir::separator() + suggetion,
        tr("PNG Image (*.png);;JPEG Image(*.jpg)"),
        &selectedFilter,
        QFileDialog::DontUseNativeDialog
    );
    if (!filePath.isEmpty()) {
        if (!filePath.endsWith(QLatin1String(".png"), Qt::CaseInsensitive) && !filePath.endsWith(QLatin1String(".jpg"), Qt::CaseInsensitive))
            filePath += selectedFilter.mid(selectedFilter.lastIndexOf(QLatin1Char('.')), 4);
        QFile::remove(filePath);
        if (!screenshot.save(filePath)) {
            QMessageBox::critical(this->widget(), tr("Error"), tr("Could not save screenshot '%1'.").arg(filePath));
        } else {
            addToResourcePool(filePath);
        }
        
        settings.setValue(QLatin1String("previousDir"), QFileInfo(filePath).absolutePath());
    }

    settings.endGroup();
}

void QnWorkbenchActionHandler::at_userSettingsAction_triggered() {
    QnUserResourcePtr user = menu()->currentResourceTarget(sender()).dynamicCast<QnUserResource>();
    if(!user)
        return;

    Qn::Permissions permissions = accessController()->permissions(user);
    if(!(permissions & Qn::ReadPermission))
        return;

    QScopedPointer<QnUserSettingsDialog> dialog(new QnUserSettingsDialog(context(), widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setWindowTitle(tr("User Settings"));

    QnUserSettingsDialog::ElementFlags flags = 
        ((permissions & Qn::ReadPermission) ? QnUserSettingsDialog::Visible : 0) | 
        ((permissions & Qn::WritePermission) ? QnUserSettingsDialog::Editable : 0);

    QnUserSettingsDialog::ElementFlags loginFlags = 
        ((permissions & Qn::ReadPermission) ? QnUserSettingsDialog::Visible : 0) | 
        ((permissions & Qn::WriteLoginPermission) ? QnUserSettingsDialog::Editable : 0);

    QnUserSettingsDialog::ElementFlags passwordFlags = 
        ((permissions & Qn::WritePasswordPermission) ? QnUserSettingsDialog::Visible : 0) | /* There is no point to display flag edit field if password cannot be changed. */
        ((permissions & Qn::WritePasswordPermission) ? QnUserSettingsDialog::Editable : 0);
    passwordFlags &= flags;

    QnUserSettingsDialog::ElementFlags accessRightsFlags = 
        ((permissions & Qn::ReadPermission) ? QnUserSettingsDialog::Visible : 0) | 
        ((permissions & Qn::WriteAccessRightsPermission) ? QnUserSettingsDialog::Editable : 0);
    accessRightsFlags &= flags;

    dialog->setElementFlags(QnUserSettingsDialog::Login, loginFlags);
    dialog->setElementFlags(QnUserSettingsDialog::Password, passwordFlags);
    dialog->setElementFlags(QnUserSettingsDialog::AccessRights, accessRightsFlags);
    
    if(user == context()->user()) {
        dialog->setElementFlags(QnUserSettingsDialog::CurrentPassword, passwordFlags);
        dialog->setCurrentPassword(qnSettings->lastUsedConnection().url.password()); // TODO: This is a totally evil hack. Store password hash/salt in user.
    } else {
        dialog->setElementFlags(QnUserSettingsDialog::CurrentPassword, 0);
    }

    QString oldPassword = user->getPassword();
    user->setPassword(QLatin1String("******"));

    dialog->setUser(user);
    if(!dialog->exec())
        return;

    user->setPassword(oldPassword);

    if(!dialog->hasChanges())
        return;

    if(permissions & Qn::SavePermission) {
        dialog->submitToResource();
        connection()->saveAsync(user, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));

        QString newPassword = user->getPassword();
        if(newPassword != oldPassword) {
            /* Password was changed. Change it in global settings and hope for the best. */
            QnSettings::ConnectionData data = qnSettings->lastUsedConnection();
            data.url.setPassword(newPassword);
            qnSettings->setLastUsedConnection(data);

            QnAppServerConnectionFactory::setDefaultUrl(data.url);
        }
    }
}

void QnWorkbenchActionHandler::at_resources_saved(int status, const QByteArray& errorString, const QnResourceList &resources, int handle) {
    if(status == 0)
        return;

    QnLayoutResourceList layoutResources = QnResourceCriterion::filter<QnLayoutResource>(resources);
    QnLayoutResourceList reopeningLayoutResources;
    foreach(const QnLayoutResourcePtr &layoutResource, layoutResources)
        if(snapshotManager()->isLocal(layoutResource) && !QnWorkbenchLayout::instance(layoutResource))
            reopeningLayoutResources.push_back(layoutResource);

    if(!reopeningLayoutResources.empty()) {
        int button = QnResourceListDialog::exec(
            widget(),
            resources,
            tr("Error"),
            tr("Could not save the following %n layout(s) to application server.", NULL, reopeningLayoutResources.size()),
            tr("Do you want to restore these %n layout(s)?", NULL, reopeningLayoutResources.size()),
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
    } else {
        QnResourceListDialog::exec(
            widget(),
            resources,
            tr("Error"),
            tr("Could not save the following %n items to application server.", NULL, resources.size()),
            tr("Error description: \n%1").arg(QLatin1String(errorString.data())),
            QDialogButtonBox::Ok
        );
    }
}

void QnWorkbenchActionHandler::at_resource_deleted(int status, const QByteArray &data, const QByteArray &errorString, int handle) {
    if(status == 0)   
        return;

    QMessageBox::critical(widget(), tr(""), tr("Could not delete resource from application server. \n\nError description: '%2'").arg(QLatin1String(errorString.data())));
}

void QnWorkbenchActionHandler::at_resources_statusSaved(int status, const QByteArray &errorString, const QnResourceList &resources, const QList<int> &oldStatuses) {
    if(status == 0 || resources.isEmpty())
        return;

    QnResourceListDialog::exec(
        widget(),
        resources,
        tr("Error"),
        tr("Could not save changes made to the following %n resource(s).", NULL, resources.size()),
        tr("Error description:\n%1").arg(QLatin1String(errorString.constData())),
        QDialogButtonBox::Ok
    );

    for(int i = 0; i < resources.size(); i++)
        resources[i]->setStatus(static_cast<QnResource::Status>(oldStatuses[i]));
}

