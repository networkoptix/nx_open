#include "workbench_action_handler.h"

#include <cassert>

#include <QtCore/QProcess>
#include <QtCore/QSettings>

#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>
#include <QtGui/QImage>
#include <QtGui/QWhatsThis>

#include <utils/common/environment.h>
#include <utils/common/delete_later.h>
#include <utils/common/mime_data.h>
#include <utils/common/event_processors.h>
#include <utils/common/string.h>
#include <utils/common/time.h>

#include <core/resource_managment/resource_discovery_manager.h>
#include <core/resource_managment/resource_pool.h>

#include <api/session_manager.h>

#include <device_plugins/server_camera/appserver.h>

#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <camera/resource_display.h>
#include <camera/cam_display.h>
#include <camera/video_camera.h>

#include <client/client_connection_data.h>

#include <recording/time_period_list.h>

#include <ui/style/skin.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_parameter_types.h>
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
#include <ui/dialogs/preferences_dialog.h>
#include <ui/dialogs/camera_addition_dialog.h>
#include <ui/dialogs/progress_dialog.h>
#include <ui/dialogs/business_rules_dialog.h>
#include <youtube/youtubeuploaddialog.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_synchronizer.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_resource.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_navigator.h>

#include <ui/workbench/watchers/workbench_panic_watcher.h>
#include <ui/workbench/watchers/workbench_schedule_watcher.h>
#include <ui/workbench/watchers/workbench_update_watcher.h>
#include <ui/workbench/watchers/workbench_user_layout_count_watcher.h>

#include "client_message_processor.h"
#include "file_processor.h"
#include "version.h"

// TODO: remove this include
#include "../extensions/workbench_stream_synchronizer.h"
#include "utils/common/synctime.h"
#include "camera/caching_time_period_loader.h"
#include "launcher/nov_launcher.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "core/resource/resource_directory_browser.h"
#include "../watchers/workbench_server_time_watcher.h"



// -------------------------------------------------------------------------- //
// QnResourceStatusReplyProcessor
// -------------------------------------------------------------------------- //
detail::QnResourceStatusReplyProcessor::QnResourceStatusReplyProcessor(QnWorkbenchActionHandler *handler, const QnVirtualCameraResourceList &resources, const QList<int> &oldDisabledFlags):
    m_handler(handler),
    m_resources(resources),
    m_oldDisabledFlags(oldDisabledFlags)
{
    assert(oldDisabledFlags.size() == resources.size());
}

void detail::QnResourceStatusReplyProcessor::at_replyReceived(int status, const QByteArray& errorString, const QnResourceList &resources, int handle) {
    Q_UNUSED(handle);

    if(m_handler)
        m_handler.data()->at_resources_statusSaved(status, errorString, resources, m_oldDisabledFlags);
    
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
// QnConnectReplyProcessor
// -------------------------------------------------------------------------- //
detail::QnConnectReplyProcessor::QnConnectReplyProcessor(QObject *parent): 
    QObject(parent),
    m_handle(0),
    m_status(0)
{}

void detail::QnConnectReplyProcessor::at_replyReceived(int status, const QByteArray &errorString, const QnConnectInfoPtr &connectInfo, int handle) {
    m_status = status;
    m_errorString = errorString;
    m_connectInfo = connectInfo;
    m_handle = handle;

    emit finished(status, errorString, connectInfo, handle);
}


// -------------------------------------------------------------------------- //
// QnWorkbenchActionHandler
// -------------------------------------------------------------------------- //
QnWorkbenchActionHandler::QnWorkbenchActionHandler(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_selectionUpdatePending(false),
    m_selectionScope(Qn::SceneScope),
    m_layoutExportCamera(0),
    m_exportedCamera(0),
    m_tourTimer(new QTimer())
{
    connect(m_tourTimer,                                        SIGNAL(timeout()),                              this,   SLOT(at_tourTimer_timeout()));
    connect(workbench(),                                        SIGNAL(itemChanged(Qn::ItemRole)),              this,   SLOT(at_workbench_itemChanged(Qn::ItemRole)));
    connect(context(),                                          SIGNAL(userChanged(const QnUserResourcePtr &)), this,   SLOT(at_context_userChanged(const QnUserResourcePtr &)));
    connect(context(),                                          SIGNAL(userChanged(const QnUserResourcePtr &)), this,   SLOT(submitDelayedDrops()), Qt::QueuedConnection);
    connect(context(),                                          SIGNAL(userChanged(const QnUserResourcePtr &)), this,   SLOT(updateCameraSettingsEditibility()));
    connect(QnClientMessageProcessor::instance(),               SIGNAL(connectionClosed()),                     this,   SLOT(at_eventManager_connectionClosed()));
    connect(QnClientMessageProcessor::instance(),               SIGNAL(connectionOpened()),                     this,   SLOT(at_eventManager_connectionOpened()));

    /* We're using queued connection here as modifying a field in its change notification handler may lead to problems. */
    connect(workbench(),                                        SIGNAL(layoutsChanged()),                       this,   SLOT(at_workbench_layoutsChanged()), Qt::QueuedConnection);
    connect(workbench(),                                        SIGNAL(cellAspectRatioChanged()),               this,   SLOT(at_workbench_cellAspectRatioChanged()));
    connect(workbench(),                                        SIGNAL(cellSpacingChanged()),                   this,   SLOT(at_workbench_cellSpacingChanged()));

    connect(action(Qn::MainMenuAction),                         SIGNAL(triggered()),    this,   SLOT(at_mainMenuAction_triggered()));
    connect(action(Qn::OpenCurrentUserLayoutMenu),              SIGNAL(triggered()),    this,   SLOT(at_openCurrentUserLayoutMenuAction_triggered()));
    connect(action(Qn::DebugIncrementCounterAction),            SIGNAL(triggered()),    this,   SLOT(at_debugIncrementCounterAction_triggered()));
    connect(action(Qn::DebugDecrementCounterAction),            SIGNAL(triggered()),    this,   SLOT(at_debugDecrementCounterAction_triggered()));
    connect(action(Qn::DebugShowResourcePoolAction),            SIGNAL(triggered()),    this,   SLOT(at_debugShowResourcePoolAction_triggered()));
    connect(action(Qn::AboutAction),                            SIGNAL(triggered()),    this,   SLOT(at_aboutAction_triggered()));
    connect(action(Qn::SystemSettingsAction),                   SIGNAL(triggered()),    this,   SLOT(at_systemSettingsAction_triggered()));
    connect(action(Qn::OpenFileAction),                         SIGNAL(triggered()),    this,   SLOT(at_openFileAction_triggered()));
    connect(action(Qn::OpenLayoutAction),                       SIGNAL(triggered()),    this,   SLOT(at_openLayoutAction_triggered()));
    connect(action(Qn::OpenFolderAction),                       SIGNAL(triggered()),    this,   SLOT(at_openFolderAction_triggered()));
    connect(action(Qn::ConnectToServerAction),                  SIGNAL(triggered()),    this,   SLOT(at_connectToServerAction_triggered()));
    connect(action(Qn::GetMoreLicensesAction),                  SIGNAL(triggered()),    this,   SLOT(at_getMoreLicensesAction_triggered()));
    connect(action(Qn::ReconnectAction),                        SIGNAL(triggered()),    this,   SLOT(at_reconnectAction_triggered()));
    connect(action(Qn::DisconnectAction),                       SIGNAL(triggered()),    this,   SLOT(at_disconnectAction_triggered()));
    connect(action(Qn::NextLayoutAction),                       SIGNAL(triggered()),    this,   SLOT(at_nextLayoutAction_triggered()));
    connect(action(Qn::PreviousLayoutAction),                   SIGNAL(triggered()),    this,   SLOT(at_previousLayoutAction_triggered()));
    connect(action(Qn::OpenInLayoutAction),                     SIGNAL(triggered()),    this,   SLOT(at_openInLayoutAction_triggered()));
    connect(action(Qn::OpenInCurrentLayoutAction),              SIGNAL(triggered()),    this,   SLOT(at_openInCurrentLayoutAction_triggered()));
    connect(action(Qn::OpenInNewLayoutAction),                  SIGNAL(triggered()),    this,   SLOT(at_openInNewLayoutAction_triggered()));
    connect(action(Qn::OpenInNewWindowAction),                  SIGNAL(triggered()),    this,   SLOT(at_openInNewWindowAction_triggered()));
    connect(action(Qn::OpenSingleLayoutAction),                 SIGNAL(triggered()),    this,   SLOT(at_openLayoutsAction_triggered()));
    connect(action(Qn::OpenMultipleLayoutsAction),              SIGNAL(triggered()),    this,   SLOT(at_openLayoutsAction_triggered()));
    connect(action(Qn::OpenAnyNumberOfLayoutsAction),           SIGNAL(triggered()),    this,   SLOT(at_openLayoutsAction_triggered()));
    connect(action(Qn::OpenNewWindowLayoutsAction),             SIGNAL(triggered()),    this,   SLOT(at_openNewWindowLayoutsAction_triggered()));
    connect(action(Qn::OpenNewTabAction),                       SIGNAL(triggered()),    this,   SLOT(at_openNewTabAction_triggered()));
    connect(action(Qn::OpenNewWindowAction),                    SIGNAL(triggered()),    this,   SLOT(at_openNewWindowAction_triggered()));
    connect(action(Qn::SaveLayoutAction),                       SIGNAL(triggered()),    this,   SLOT(at_saveLayoutAction_triggered()));
    connect(action(Qn::SaveLayoutAsAction),                     SIGNAL(triggered()),    this,   SLOT(at_saveLayoutAsAction_triggered()));
    connect(action(Qn::SaveLayoutForCurrentUserAsAction),       SIGNAL(triggered()),    this,   SLOT(at_saveLayoutForCurrentUserAsAction_triggered()));
    connect(action(Qn::SaveCurrentLayoutAction),                SIGNAL(triggered()),    this,   SLOT(at_saveCurrentLayoutAction_triggered()));
    connect(action(Qn::SaveCurrentLayoutAsAction),              SIGNAL(triggered()),    this,   SLOT(at_saveCurrentLayoutAsAction_triggered()));
    connect(action(Qn::CloseLayoutAction),                      SIGNAL(triggered()),    this,   SLOT(at_closeLayoutAction_triggered()));
    connect(action(Qn::CloseAllButThisLayoutAction),            SIGNAL(triggered()),    this,   SLOT(at_closeAllButThisLayoutAction_triggered()));
    connect(action(Qn::UserSettingsAction),                     SIGNAL(triggered()),    this,   SLOT(at_userSettingsAction_triggered()));
    connect(action(Qn::CameraSettingsAction),                   SIGNAL(triggered()),    this,   SLOT(at_cameraSettingsAction_triggered()));
    connect(action(Qn::OpenInCameraSettingsDialogAction),       SIGNAL(triggered()),    this,   SLOT(at_cameraSettingsAction_triggered()));
    connect(action(Qn::ClearCameraSettingsAction),              SIGNAL(triggered()),    this,   SLOT(at_clearCameraSettingsAction_triggered()));
    connect(action(Qn::SelectionChangeAction),                  SIGNAL(triggered()),    this,   SLOT(at_selectionChangeAction_triggered()));
    connect(action(Qn::ServerAddCameraManuallyAction),          SIGNAL(triggered()),    this,   SLOT(at_serverAddCameraManuallyAction_triggered()));
    connect(action(Qn::ServerSettingsAction),                   SIGNAL(triggered()),    this,   SLOT(at_serverSettingsAction_triggered()));
    connect(action(Qn::YouTubeUploadAction),                    SIGNAL(triggered()),    this,   SLOT(at_youtubeUploadAction_triggered()));
    connect(action(Qn::EditTagsAction),                         SIGNAL(triggered()),    this,   SLOT(at_editTagsAction_triggered()));
    connect(action(Qn::OpenInFolderAction),                     SIGNAL(triggered()),    this,   SLOT(at_openInFolderAction_triggered()));
    connect(action(Qn::DeleteFromDiskAction),                   SIGNAL(triggered()),    this,   SLOT(at_deleteFromDiskAction_triggered()));
    connect(action(Qn::RemoveLayoutItemAction),                 SIGNAL(triggered()),    this,   SLOT(at_removeLayoutItemAction_triggered()));
    connect(action(Qn::RemoveFromServerAction),                 SIGNAL(triggered()),    this,   SLOT(at_removeFromServerAction_triggered()));
    connect(action(Qn::NewUserAction),                          SIGNAL(triggered()),    this,   SLOT(at_newUserAction_triggered()));
    connect(action(Qn::NewUserLayoutAction),                    SIGNAL(triggered()),    this,   SLOT(at_newUserLayoutAction_triggered()));
    connect(action(Qn::RenameAction),                           SIGNAL(triggered()),    this,   SLOT(at_renameAction_triggered()));
    connect(action(Qn::DropResourcesAction),                    SIGNAL(triggered()),    this,   SLOT(at_dropResourcesAction_triggered()));
    connect(action(Qn::DelayedDropResourcesAction),             SIGNAL(triggered()),    this,   SLOT(at_delayedDropResourcesAction_triggered()));
    connect(action(Qn::InstantDropResourcesAction),             SIGNAL(triggered()),    this,   SLOT(at_instantDropResourcesAction_triggered()));
    connect(action(Qn::DropResourcesIntoNewLayoutAction),       SIGNAL(triggered()),    this,   SLOT(at_dropResourcesIntoNewLayoutAction_triggered()));
    connect(action(Qn::MoveCameraAction),                       SIGNAL(triggered()),    this,   SLOT(at_moveCameraAction_triggered()));
    connect(action(Qn::TakeScreenshotAction),                   SIGNAL(triggered()),    this,   SLOT(at_takeScreenshotAction_triggered()));
    connect(action(Qn::ExitAction),                             SIGNAL(triggered()),    this,   SLOT(at_exitAction_triggered()));
    connect(action(Qn::ExportTimeSelectionAction),              SIGNAL(triggered()),    this,   SLOT(at_exportTimeSelectionAction_triggered()));
    connect(action(Qn::ExportLayoutAction),                     SIGNAL(triggered()),    this,   SLOT(at_exportLayoutAction_triggered()));
    connect(action(Qn::ThumbnailsSearchAction),                 SIGNAL(triggered()),    this,   SLOT(at_thumbnailsSearchAction_triggered()));
    connect(action(Qn::SetCurrentLayoutAspectRatio4x3Action),   SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutAspectRatio4x3Action_triggered()));
    connect(action(Qn::SetCurrentLayoutAspectRatio16x9Action),  SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutAspectRatio16x9Action_triggered()));
    connect(action(Qn::SetCurrentLayoutItemSpacing0Action),     SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutItemSpacing0Action_triggered()));
    connect(action(Qn::SetCurrentLayoutItemSpacing10Action),    SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutItemSpacing10Action_triggered()));
    connect(action(Qn::SetCurrentLayoutItemSpacing20Action),    SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutItemSpacing20Action_triggered()));
    connect(action(Qn::SetCurrentLayoutItemSpacing30Action),    SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutItemSpacing30Action_triggered()));
    connect(action(Qn::Rotate0Action),                          SIGNAL(triggered()),    this,   SLOT(at_rotate0Action_triggered()));
    connect(action(Qn::Rotate90Action),                         SIGNAL(triggered()),    this,   SLOT(at_rotate90Action_triggered()));
    connect(action(Qn::Rotate180Action),                        SIGNAL(triggered()),    this,   SLOT(at_rotate180Action_triggered()));
    connect(action(Qn::Rotate270Action),                        SIGNAL(triggered()),    this,   SLOT(at_rotate270Action_triggered()));
    connect(action(Qn::RadassAutoAction),                       SIGNAL(triggered()),    this,   SLOT(at_radassAutoAction_triggered()));
    connect(action(Qn::RadassLowAction),                        SIGNAL(triggered()),    this,   SLOT(at_radassLowAction_triggered()));
    connect(action(Qn::RadassHighAction),                       SIGNAL(triggered()),    this,   SLOT(at_radassHighAction_triggered()));
    connect(action(Qn::WhatsThisAction),                        SIGNAL(triggered()),    this,   SLOT(at_whatsThisAction_triggered()));

    connect(action(Qn::TogglePanicModeAction),                  SIGNAL(toggled(bool)),  this,   SLOT(at_togglePanicModeAction_toggled(bool)));
    connect(action(Qn::ToggleTourModeAction),                   SIGNAL(toggled(bool)),  this,   SLOT(at_toggleTourAction_toggled(bool)));
    connect(action(Qn::ToggleTourModeHotkeyAction),             SIGNAL(triggered()),    action(Qn::ToggleTourModeAction),   SLOT(toggle()));
    connect(context()->instance<QnWorkbenchPanicWatcher>(),     SIGNAL(panicModeChanged()), this, SLOT(at_panicWatcher_panicModeChanged()));
    connect(context()->instance<QnWorkbenchScheduleWatcher>(),  SIGNAL(scheduleEnabledChanged()), this, SLOT(at_scheduleWatcher_scheduleEnabledChanged()));
    connect(context()->instance<QnWorkbenchUpdateWatcher>(),    SIGNAL(availableUpdateChanged()), this, SLOT(at_updateWatcher_availableUpdateChanged()));
    //connect(context()->instance<QnWorkbenchUserLayoutCountWatcher>(), SIGNAL(layoutCountChangeD()), this, SLOT(at_layoutCountWatcher_layoutCountChanged())); // TODO: not needed?

    /*
    SignalingInstrument *activityInstrument = new SignalingInstrument(
        Instrument::makeSet(QEvent::MouseButtonRelease), 
        Instrument::makeSet(QEvent::KeyRelease),
        Instrument::makeSet(),
        Instrument::makeSet(),
        this
    );
    foreach(InstrumentManager *manager, InstrumentManager::managersOf(display()->scene())) {
        manager->installInstrument(activityInstrument);
        break;
    }
    connect(activityInstrument, SIGNAL(activated(QGraphicsView *, QEvent *)), this, SLOT(at_activityInstrument_activated()));
    connect(activityInstrument, SIGNAL(activated(QWidget *, QEvent *)), this, SLOT(at_activityInstrument_activated()));
    */

    /* Run handlers that update state. */
    at_eventManager_connectionClosed();
    at_panicWatcher_panicModeChanged();
    at_scheduleWatcher_scheduleEnabledChanged();


    at_updateWatcher_availableUpdateChanged();
}

QnWorkbenchActionHandler::~QnWorkbenchActionHandler() {
    disconnect(context(), NULL, this, NULL);
    disconnect(workbench(), NULL, this, NULL);

    foreach(QAction *action, menu()->actions())
        disconnect(action, NULL, this, NULL);

    /* Clean up. */
    if(m_mainMenu)
        delete m_mainMenu.data();

    if(m_currentUserLayoutsMenu)
        delete m_currentUserLayoutsMenu.data();

    if(cameraSettingsDialog())
        delete cameraSettingsDialog();

    if (m_layoutExportCamera)
        m_layoutExportCamera->deleteLater();
}

QnAppServerConnectionPtr QnWorkbenchActionHandler::connection() const {
    return QnAppServerConnectionFactory::createConnection();
}

QWidget *QnWorkbenchActionHandler::widget() const {
    return m_widget.data();
}

QString QnWorkbenchActionHandler::newLayoutName(const QnUserResourcePtr &user) const {
    const QString zeroName = tr("New layout");
    const QString nonZeroName = tr("New layout %1");
    QRegExp pattern = QRegExp(tr("New layout ?([0-9]+)?"));

    QStringList layoutNames;

    QnUserResourcePtr parent = !user.isNull() ? user : context()->user();
    QnId parentId = parent ? parent->getId() : QnId();
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
    
    return snapshotManager()->flags(layoutResource) == Qn::ResourceIsLocal; /* Local, not changed and not being saved. */
}

void QnWorkbenchActionHandler::addToLayout(const QnLayoutResourcePtr &layout, const QnResourcePtr &resource, bool usePosition, const QPointF &position) const {
    QnLayoutItemData data;
    data.resource.id = resource->getId();
    data.resource.path = resource->getUniqueId();
    data.uuid = QUuid::createUuid();
    data.flags = Qn::PendingGeometryAdjustment;
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

        Qn::ResourceSavingFlags flags = snapshotManager()->flags(resource);
        askable = flags == (Qn::ResourceIsChanged | Qn::ResourceIsLocal); /* Changed, local, not being saved. */
        changed = flags & Qn::ResourceIsChanged;
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
            QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel,
            false
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
    if(!saveResources.empty()) {
        QnLayoutResourceList fileResources, normalResources;
        foreach(const QnLayoutResourcePtr &resource, saveResources) {
            if(snapshotManager()->isFile(resource)) {
                fileResources.push_back(resource);
            } else {
                normalResources.push_back(resource);
            }
        }
        
        if(!normalResources.isEmpty())
            snapshotManager()->save(normalResources, object, slot);

        foreach(const QnLayoutResourcePtr &fileResource, fileResources) {
            if (validateItemTypes(fileResource)) { // TODO: and if not?
                bool isReadOnly = !(accessController()->permissions(fileResource) & Qn::WritePermission);
                saveLayoutToLocalFile(fileResource->getLocalRange(), fileResource, fileResource->getUrl(), LayoutExport_LocalSave, isReadOnly); // overwrite layout file
            }
        }

        if(normalResources.isEmpty()) {
            QByteArray method(slot && *slot ? slot + 1 : slot);
            int index = method.indexOf('(');
            if(index != -1)
                method.truncate(index);

            QMetaObject::invokeMethod(
                object, 
                method.constData(), 
                Qt::QueuedConnection, 
                Q_ARG(int, 0),
                Q_ARG(QByteArray, QByteArray()),
                Q_ARG(QnResourceList, QnResourceList()),
                Q_ARG(int, 0)
            );
        }
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

bool QnWorkbenchActionHandler::closeAllLayouts(bool waitForReply) {
    return closeLayouts(resourcePool()->getResources().filtered<QnLayoutResource>(), waitForReply);
}

void QnWorkbenchActionHandler::openResourcesInNewWindow(const QnResourceList &resources){
    QMimeData mimeData;
    QnWorkbenchResource::serializeResources(resources, QnWorkbenchResource::resourceMimeTypes(), &mimeData);
    QnMimeData data(&mimeData);
    QByteArray serializedData;
    QDataStream stream(&serializedData, QIODevice::WriteOnly);
    stream << data;

    QStringList arguments;
    if (context()->user())
        arguments << QLatin1String("--delayed-drop");
    else
        arguments << QLatin1String("--instant-drop");
    arguments << QLatin1String(serializedData.toBase64().data());

    openNewWindow(arguments);
}

void QnWorkbenchActionHandler::openNewWindow(const QStringList &args) {
    QStringList arguments = args;
    arguments << QLatin1String("--no-single-application");
    arguments << QLatin1String("--auth");
    arguments << QLatin1String(qnSettings->lastUsedConnection().url.toEncoded());

    /* For now, simply open it at another screen. Don't account for 3+ monitor setups. */
    if(widget()) {
        int screen = qApp->desktop()->screenNumber(widget());
        screen = (screen + 1) % qApp->desktop()->screenCount();

        arguments << QLatin1String("--screen");
        arguments << QString::number(screen);
    }

    QProcess::startDetached(qApp->applicationFilePath(), arguments);
}

void QnWorkbenchActionHandler::saveCameraSettingsFromDialog(bool checkControls) {
    if(!cameraSettingsDialog())
        return;

    bool hasDbChanges = cameraSettingsDialog()->widget()->hasDbChanges();
    bool hasCameraChanges = cameraSettingsDialog()->widget()->hasCameraChanges();

    if (checkControls && cameraSettingsDialog()->widget()->hasControlsChanges()){
        QString message = tr(" Your recording changes have not been saved. Pick desired Recording Type, FPS, and Quality and mark the changes on the schedule. Press APPLY to save changes.");
        int button = QMessageBox::warning(widget(), tr("Changes are not applied"), message,
                             QMessageBox::Retry, QMessageBox::Ignore);
        if (button == QMessageBox::Retry){
            cameraSettingsDialog()->ignoreAcceptOnce();
            return;
        }
    }

    if (!hasDbChanges && !hasCameraChanges && !cameraSettingsDialog()->widget()->hasAnyCameraChanges()) {
        return;
    }

    QnVirtualCameraResourceList cameras = cameraSettingsDialog()->widget()->cameras();
    if(cameras.empty())
        return;
    
    /* Dialog will be shown inside */
    if (!cameraSettingsDialog()->widget()->isValidMotionRegion())
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

    if (hasDbChanges) {
        connection()->saveAsync(cameras, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
    }

    if (hasCameraChanges) {
        saveAdvancedCameraSettingsAsync(cameras);
    }
}

void QnWorkbenchActionHandler::saveAdvancedCameraSettingsAsync(QnVirtualCameraResourceList cameras)
{
    if (cameraSettingsDialog()->widget()->mode() != QnCameraSettingsWidget::SingleMode || cameras.size() != 1)
    {
        //Advanced camera settings must be available only for single mode
        Q_ASSERT(false);
    }

    QnVirtualCameraResourcePtr cameraPtr = cameras.front();
    QnMediaServerConnectionPtr serverConnectionPtr = cameraSettingsDialog()->widget()->getServerConnection();
    if (serverConnectionPtr.isNull())
    {
        QString error = QString::fromLatin1("Connection refused");

        QString failedParams;
        QList< QPair< QString, QVariant> >::ConstIterator it =
            cameraSettingsDialog()->widget()->getModifiedAdvancedParams().begin();
        for (; it != cameraSettingsDialog()->widget()->getModifiedAdvancedParams().end(); ++it)
        {
            QString formattedParam(it->first.right(it->first.length() - 2));
            failedParams += QString::fromLatin1("\n");
            failedParams += formattedParam.replace(QString::fromLatin1("%%"), QString::fromLatin1("->"));
        }

        if (!failedParams.isEmpty()) {
            QMessageBox::warning(
                widget(),
                tr("Currently parameters can't be saved."),
                tr("Failed to save the following parameters (%1):\n%2").arg(error, failedParams),
                1, 0);

            cameraSettingsDialog()->widget()->updateFromResources();
        }

        return;
    }

    qRegisterMetaType<QList<QPair<QString, bool> > >("QList<QPair<QString, bool> >"); // TODO: evil!
    serverConnectionPtr->asyncSetParam(cameraPtr, cameraSettingsDialog()->widget()->getModifiedAdvancedParams(),
        this, SLOT(at_camera_settings_saved(int, const QList<QPair<QString, bool> >&)) );
}

void QnWorkbenchActionHandler::rotateItems(int degrees){
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();
    if(!widgets.empty()) {
        foreach(const QnResourceWidget *widget, widgets)
            widget->item()->setRotation(degrees);
    }
}

void QnWorkbenchActionHandler::setItemsResolutionMode(Qn::ResolutionMode resolutionMode) {
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();
    if(!widgets.empty()) {
        foreach(QnResourceWidget *widget, widgets)
            if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
                mediaWidget->setResolutionMode(resolutionMode);
    }
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

    menu()->trigger(Qn::OpenInCameraSettingsDialogAction, QnActionParameters(provider->currentTarget(scope)));
}

void QnWorkbenchActionHandler::submitDelayedDrops() {
    if(!context()->user())
        return;

    foreach(const QnMimeData &data, m_delayedDrops) {
        QMimeData mimeData;
        data.toMimeData(&mimeData);

        QnResourceList resources = QnWorkbenchResource::deserializeResources(&mimeData);
        QnLayoutResourceList layouts = resources.filtered<QnLayoutResource>();
        if (!layouts.isEmpty()){
            workbench()->clear();
            menu()->trigger(Qn::OpenAnyNumberOfLayoutsAction, layouts);
        } else {
            menu()->trigger(Qn::OpenInCurrentLayoutAction, resources);
        }
    }

    m_delayedDrops.clear();
}

void QnWorkbenchActionHandler::submitInstantDrop(QnMimeData &data) {
    QMimeData mimeData;
    data.toMimeData(&mimeData);

    QnResourceList resources = QnWorkbenchResource::deserializeResources(&mimeData);
    menu()->trigger(Qn::OpenInCurrentLayoutAction, resources);
}



// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchActionHandler::at_context_userChanged(const QnUserResourcePtr &user) {
    if(!user)
        return;

    /* Open all user's layouts. */
    //if(qnSettings->isLayoutsOpenedOnLogin()) {
        //QnLayoutResourceList layouts = context()->resourcePool()->getResourcesWithParentId(user->getId()).filtered<QnLayoutResource>();
        //menu()->trigger(Qn::OpenAnyNumberOfLayoutsAction, layouts);
    //}
    QnWorkbenchState state = qnSettings->userWorkbenchStates().value(user->getName());
    workbench()->update(state);
    
    /* Delete empty orphaned layouts, move non-empty to the new user. */
    foreach(const QnResourcePtr &resource, context()->resourcePool()->getResourcesWithParentId(QnId())) {
        if(QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>()) {
            if(snapshotManager()->isLocal(layout)) {
                if(layout->getItems().empty()) {
                    resourcePool()->removeResource(layout);
                } else {
                    layout->setParentId(user->getId());
                }
            }
        }
    }

    /* Close all other layouts. */
    foreach(QnWorkbenchLayout *layout, workbench()->layouts()) {
        QnLayoutResourcePtr resource = layout->resource();
        if(resource->getParentId() != user->getId())
            workbench()->removeLayout(layout);
    }
}

void QnWorkbenchActionHandler::at_workbench_layoutsChanged() {
    if(!workbench()->layouts().empty())
        return;

    menu()->trigger(Qn::OpenNewTabAction);
}

void QnWorkbenchActionHandler::at_workbench_cellAspectRatioChanged() {
    qreal value = workbench()->currentLayout()->cellAspectRatio();

    if (qFuzzyCompare(4.0 / 3.0, value))
        action(Qn::SetCurrentLayoutAspectRatio4x3Action)->setChecked(true);
    else
        action(Qn::SetCurrentLayoutAspectRatio16x9Action)->setChecked(true); //default value
}

void QnWorkbenchActionHandler::at_workbench_cellSpacingChanged() {
    qreal value = workbench()->currentLayout()->cellSpacing().width();

    if (qFuzzyCompare(0.0, value))
        action(Qn::SetCurrentLayoutItemSpacing0Action)->setChecked(true);
    else if (qFuzzyCompare(0.2, value))
        action(Qn::SetCurrentLayoutItemSpacing20Action)->setChecked(true);
    else if (qFuzzyCompare(0.3, value))
        action(Qn::SetCurrentLayoutItemSpacing30Action)->setChecked(true);
    else
        action(Qn::SetCurrentLayoutItemSpacing10Action)->setChecked(true); //default value
}

void QnWorkbenchActionHandler::at_eventManager_connectionClosed() {
    action(Qn::ConnectToServerAction)->setIcon(qnSkin->icon("titlebar/disconnected.png"));
    action(Qn::ConnectToServerAction)->setText(tr("Connect to Server..."));
}

void QnWorkbenchActionHandler::at_eventManager_connectionOpened() {
    action(Qn::ConnectToServerAction)->setIcon(qnSkin->icon("titlebar/connected.png"));
    action(Qn::ConnectToServerAction)->setText(tr("Connect to Another Server...")); // TODO: use conditional texts? 
}

void QnWorkbenchActionHandler::at_mainMenuAction_triggered() {
    m_mainMenu = menu()->newMenu(Qn::MainScope);

    action(Qn::MainMenuAction)->setMenu(m_mainMenu.data());
}

void QnWorkbenchActionHandler::at_openCurrentUserLayoutMenuAction_triggered() {
    m_currentUserLayoutsMenu = menu()->newMenu(Qn::OpenCurrentUserLayoutMenu, Qn::TitleBarScope);

    action(Qn::OpenCurrentUserLayoutMenu)->setMenu(m_currentUserLayoutsMenu.data());
}

void QnWorkbenchActionHandler::at_layoutCountWatcher_layoutCountChanged() {
    action(Qn::OpenCurrentUserLayoutMenu)->setEnabled(context()->instance<QnWorkbenchUserLayoutCountWatcher>()->layoutCount() > 0);
}

void QnWorkbenchActionHandler::at_debugIncrementCounterAction_triggered() {
    qnSettings->setDebugCounter(qnSettings->debugCounter() + 1);

    QScopedPointer<QnBusinessRulesDialog> dialog(new QnBusinessRulesDialog(widget()));
    dialog->exec();
}

void QnWorkbenchActionHandler::at_debugDecrementCounterAction_triggered() {
    qnSettings->setDebugCounter(qnSettings->debugCounter() - 1);
}

void QnWorkbenchActionHandler::at_debugShowResourcePoolAction_triggered() {
    QScopedPointer<QnResourceListDialog> dialog(new QnResourceListDialog(widget()));
    dialog->setResources(resourcePool()->getResources());
    dialog->exec();
}

void QnWorkbenchActionHandler::at_nextLayoutAction_triggered() {
    workbench()->setCurrentLayoutIndex((workbench()->currentLayoutIndex() + 1) % workbench()->layouts().size());
}

void QnWorkbenchActionHandler::at_previousLayoutAction_triggered() {
    workbench()->setCurrentLayoutIndex((workbench()->currentLayoutIndex() - 1 + workbench()->layouts().size()) % workbench()->layouts().size());
}

void QnWorkbenchActionHandler::at_openInLayoutAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnLayoutResourcePtr layout = parameters.argument<QnLayoutResourcePtr>(Qn::LayoutParameter);
    if(!layout) {
        qnWarning("No layout provided.");
        return;
    }

    QPointF position = parameters.argument<QPointF>(Qn::GridPositionParameter);

    QnResourceWidgetList widgets = parameters.widgets();
    if(!widgets.empty() && position.isNull() && layout->getItems().empty()) {
        foreach(const QnResourceWidget *widget, widgets)
            layout->addItem(widget->item()->data());
        return;
    }

    // TODO: server & media resources only!
    QnResourceList resources = parameters.resources();
    if(!resources.isEmpty()) {
        addToLayout(layout, resources, !position.isNull(), position);
        return;
    }
}

void QnWorkbenchActionHandler::at_openInCurrentLayoutAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    parameters.setArgument(Qn::LayoutParameter, workbench()->currentLayout()->resource());
    menu()->trigger(Qn::OpenInLayoutAction, parameters);
};

void QnWorkbenchActionHandler::at_openInNewLayoutAction_triggered() {
    menu()->trigger(Qn::OpenNewTabAction);
    menu()->trigger(Qn::OpenInCurrentLayoutAction, menu()->currentParameters(sender()));
}

void QnWorkbenchActionHandler::at_openInNewWindowAction_triggered() {
    // TODO: server & media resources only!
    QnResourceList resources = menu()->currentParameters(sender()).resources();
    if(resources.isEmpty()) 
        return;
    
    openResourcesInNewWindow(resources);
}

void QnWorkbenchActionHandler::at_openLayoutsAction_triggered() {
    foreach(const QnResourcePtr &resource, menu()->currentParameters(sender()).resources()) {
        QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
        if(!layoutResource)
            continue;

        QnWorkbenchLayout *layout = QnWorkbenchLayout::instance(layoutResource);
        if(layout == NULL) {
            layout = new QnWorkbenchLayout(layoutResource, workbench());
            workbench()->addLayout(layout);
        }
        
        workbench()->setCurrentLayout(layout);
    }
}

void QnWorkbenchActionHandler::at_openNewWindowLayoutsAction_triggered() {
    // TODO: #GDM this won't work for layouts that are not saved. (de)serialization of layouts is not implemented.
    QnLayoutResourceList layouts = menu()->currentParameters(sender()).resources().filtered<QnLayoutResource>();
    if(layouts.isEmpty()) 
        return;
    openResourcesInNewWindow(layouts);
}

void QnWorkbenchActionHandler::at_openNewTabAction_triggered() {
    QnWorkbenchLayout *layout = new QnWorkbenchLayout(this);
    layout->setName(newLayoutName(QnUserResourcePtr()));

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

    if (snapshotManager()->isFile(layout)) {
        if (!validateItemTypes(layout))
            return;
        bool isReadOnly = !(accessController()->permissions(layout) & Qn::WritePermission);
        saveLayoutToLocalFile(layout->getLocalRange(), layout, layout->getUrl(), LayoutExport_LocalSave, isReadOnly); // overwrite layout file
    } else {
        snapshotManager()->save(layout, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
    }
}

void QnWorkbenchActionHandler::at_saveLayoutAction_triggered() {
    at_saveLayoutAction_triggered(menu()->currentParameters(sender()).resource().dynamicCast<QnLayoutResource>());
}

void QnWorkbenchActionHandler::at_saveCurrentLayoutAction_triggered() {
    at_saveLayoutAction_triggered(workbench()->currentLayout()->resource());
}

void QnWorkbenchActionHandler::at_saveLayoutAsAction_triggered(const QnLayoutResourcePtr &layout, const QnUserResourcePtr &user) {
    if(!layout)
        return;

    if(!user)
        return;

    if(snapshotManager()->isFile(layout)) {
        doAskNameAndExportLocalLayout(layout->getLocalRange(), layout, LayoutExport_LocalSaveAs);
    } else {
        QString name = menu()->currentParameters(sender()).argument(Qn::NameParameter).toString();
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

        newLayout = QnLayoutResourcePtr(new QnLayoutResource());
        newLayout->setGuid(QUuid::createUuid());
        newLayout->setName(name);
        newLayout->setParentId(user->getId());
        newLayout->setData(Qn::LayoutSyncStateRole, QVariant::fromValue<QnStreamSynchronizationState>(QnStreamSynchronizationState(true, DATETIME_NOW, 1.0))); // TODO: this does not belong here.
        newLayout->setCellSpacing(layout->cellSpacing());
        newLayout->setCellAspectRatio(layout->cellAspectRatio());
        context()->resourcePool()->addResource(newLayout);

        QnLayoutItemDataList items = layout->getItems().values();
        for(int i = 0; i < items.size(); i++)
            items[i].uuid = QUuid::createUuid();
        newLayout->setItems(items);

        /* If it is current layout, then roll it back and open the new one instead. */
        if(layout == workbench()->currentLayout()->resource()) {
            int index = workbench()->currentLayoutIndex();
            workbench()->insertLayout(new QnWorkbenchLayout(newLayout, this), index);
            workbench()->setCurrentLayoutIndex(index);
            workbench()->removeLayout(index + 1);

            snapshotManager()->restore(layout);
        }

        snapshotManager()->save(newLayout, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
    }
}

void QnWorkbenchActionHandler::at_saveLayoutForCurrentUserAsAction_triggered() {
    at_saveLayoutAsAction_triggered(
        menu()->currentParameters(sender()).resource().dynamicCast<QnLayoutResource>(), 
        context()->user()
    );
}

void QnWorkbenchActionHandler::at_saveLayoutAsAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    at_saveLayoutAsAction_triggered(
        parameters.resource().dynamicCast<QnLayoutResource>(), 
        parameters.argument<QnUserResourcePtr>(Qn::UserParameter)
    );
}

void QnWorkbenchActionHandler::at_saveCurrentLayoutAsAction_triggered() {
    at_saveLayoutAsAction_triggered(
        workbench()->currentLayout()->resource(),
        context()->user()
    );
}

void QnWorkbenchActionHandler::at_closeLayoutAction_triggered() {
    closeLayouts(menu()->currentParameters(sender()).layouts());
}

void QnWorkbenchActionHandler::at_closeAllButThisLayoutAction_triggered() {
    QnWorkbenchLayoutList layouts = menu()->currentParameters(sender()).layouts();
    if(layouts.empty())
        return;
    
    QnWorkbenchLayoutList layoutsToClose = workbench()->layouts();
    foreach(QnWorkbenchLayout *layout, layouts)
        layoutsToClose.removeOne(layout);

    closeLayouts(layoutsToClose);
}

void QnWorkbenchActionHandler::at_moveCameraAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnResourceList resources = parameters.resources();
    QnMediaServerResourcePtr server = parameters.argument<QnMediaServerResourcePtr>(Qn::ServerParameter);
    if(!server)
        return;
    QnVirtualCameraResourceList serverCameras = resourcePool()->getResourcesWithParentId(server->getId()).filtered<QnVirtualCameraResource>();

    QnVirtualCameraResourceList modifiedResources;
    QnResourceList errorResources;
    QList<int> oldDisabledFlags;

    foreach(const QnResourcePtr &resource, resources) {
        if(resource->getParentId() == server->getId())
            continue; /* Moving resource into its owner does nothing. */

        QnVirtualCameraResourcePtr sourceCamera = resource.dynamicCast<QnVirtualCameraResource>();
        if(!sourceCamera)
            continue;

                QString physicalId = sourceCamera->getPhysicalId();

        QnVirtualCameraResourcePtr replacedCamera;
        foreach(const QnVirtualCameraResourcePtr &otherCamera, serverCameras) {
            if(otherCamera->getPhysicalId() == physicalId) {
                replacedCamera = otherCamera;
                break;
            }
        }

        if(replacedCamera) {
            oldDisabledFlags.push_back(replacedCamera->isDisabled());
            replacedCamera->setScheduleDisabled(sourceCamera->isScheduleDisabled());
            replacedCamera->setScheduleTasks(sourceCamera->getScheduleTasks());
            replacedCamera->setAuth(sourceCamera->getAuth());
            replacedCamera->setAudioEnabled(sourceCamera->isAudioEnabled());
            replacedCamera->setMotionRegionList(sourceCamera->getMotionRegionList(), QnDomainMemory);
            replacedCamera->setName(sourceCamera->getName());
            replacedCamera->setDisabled(false);

            oldDisabledFlags.push_back(sourceCamera->isDisabled());
            sourceCamera->setDisabled(true);

            modifiedResources.push_back(sourceCamera);
            modifiedResources.push_back(replacedCamera);

            QnResourcePtr newServer = resourcePool()->getResourceById(sourceCamera->getParentId());
            if (newServer->getStatus() == QnResource::Offline)
                sourceCamera->setStatus(QnResource::Offline);
        } else {
            errorResources.push_back(resource);
        }
    }

    if(!errorResources.empty()) {
        QnResourceListDialog::exec(
            widget(),
            errorResources,
            tr("Error"),
            tr("Camera(s) cannot be moved to server '%1'. It might have been offline since the server is up.").arg(server->getName()),
            QDialogButtonBox::Ok
        );
    }

    if(!modifiedResources.empty()) {
        detail::QnResourceStatusReplyProcessor *processor = new detail::QnResourceStatusReplyProcessor(this, modifiedResources, oldDisabledFlags);
        connection()->saveAsync(modifiedResources, processor, SLOT(at_replyReceived(int, const QByteArray &, const QnResourceList &, int)));
    }
}

void QnWorkbenchActionHandler::at_dropResourcesAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnLayoutResourceList layouts = parameters.resources().filtered<QnLayoutResource>();
    foreach(QnLayoutResourcePtr r, layouts)
        parameters.resources().removeOne(r);

    if (!parameters.resources().empty())
        menu()->trigger(Qn::OpenInCurrentLayoutAction, parameters);
    if(!layouts.empty())
        menu()->trigger(Qn::OpenAnyNumberOfLayoutsAction, layouts);
}

void QnWorkbenchActionHandler::at_dropResourcesIntoNewLayoutAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnLayoutResourceList layouts = parameters.resources().filtered<QnLayoutResource>();
    if(layouts.empty()) /* That's media drop, open new layout. */
        menu()->trigger(Qn::OpenNewTabAction);

    menu()->trigger(Qn::DropResourcesAction, parameters);
}

void QnWorkbenchActionHandler::at_delayedDropResourcesAction_triggered() {
    QByteArray data = menu()->currentParameters(sender()).argument<QByteArray>(Qn::SerializedResourcesParameter);
    QDataStream stream(&data, QIODevice::ReadOnly);
    QnMimeData mimeData;
    stream >> mimeData;
    if(stream.status() != QDataStream::Ok || mimeData.formats().empty())
        return;

    m_delayedDrops.push_back(mimeData);

    submitDelayedDrops();
}

void QnWorkbenchActionHandler::at_instantDropResourcesAction_triggered() {
    QByteArray data = menu()->currentParameters(sender()).argument<QByteArray>(Qn::SerializedResourcesParameter);
    QDataStream stream(&data, QIODevice::ReadOnly);
    QnMimeData mimeData;
    stream >> mimeData;
    if(stream.status() != QDataStream::Ok || mimeData.formats().empty())
        return;
    submitInstantDrop(mimeData);
}

void QnWorkbenchActionHandler::at_openFileAction_triggered() {
    QScopedPointer<QFileDialog> dialog(new QFileDialog(widget(), tr("Open file")));
    dialog->setOption(QFileDialog::DontUseNativeDialog, true);
    dialog->setFileMode(QFileDialog::ExistingFiles);
    QStringList filters;
    //filters << tr("All Supported (*.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp *.jpg *.png *.gif *.bmp *.tiff *.layout)");
    filters << tr("All Supported (*.nov *.avi *.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp *.jpg *.png *.gif *.bmp *.tiff)");
    filters << tr("Video (*.avi *.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp)");
    filters << tr("Pictures (*.jpg *.png *.gif *.bmp *.tiff)");
    //filters << tr("Layouts (*.layout)"); // TODO
    filters << tr("All files (*.*)");
    dialog->setNameFilters(filters);
    
    if(dialog->exec())
        menu()->trigger(Qn::DropResourcesAction, addToResourcePool(dialog->selectedFiles()));
}

void QnWorkbenchActionHandler::at_openLayoutAction_triggered() {
    QScopedPointer<QFileDialog> dialog(new QFileDialog(widget(), tr("Open file")));
    dialog->setOption(QFileDialog::DontUseNativeDialog, true);
    dialog->setFileMode(QFileDialog::ExistingFiles);
    QStringList filters;
    filters << tr("All Supported (*.layout)");
    filters << tr("Layouts (*.layout)");
    filters << tr("All files (*.*)");
    dialog->setNameFilters(filters);

    if(dialog->exec())
        menu()->trigger(Qn::DropResourcesAction, addToResourcePool(dialog->selectedFiles()).filtered<QnLayoutResource>());
}

void QnWorkbenchActionHandler::at_openFolderAction_triggered() {
    QScopedPointer<QFileDialog> dialog(new QFileDialog(widget(), tr("Open file")));
    dialog->setOption(QFileDialog::DontUseNativeDialog, true);
    dialog->setFileMode(QFileDialog::Directory);
    dialog->setOptions(QFileDialog::ShowDirsOnly);
    
    if(dialog->exec())
        menu()->trigger(Qn::DropResourcesAction, addToResourcePool(dialog->selectedFiles()));
}

void QnWorkbenchActionHandler::at_aboutAction_triggered() {
    QScopedPointer<QnAboutDialog> dialog(new QnAboutDialog(widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_getMoreLicensesAction_triggered() {
    QScopedPointer<QnPreferencesDialog> dialog(new QnPreferencesDialog(context(), widget()));
    dialog->openLicensesPage();
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_systemSettingsAction_triggered() {
    QScopedPointer<QnPreferencesDialog> dialog(new QnPreferencesDialog(context(), widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_connectToServerAction_triggered() {
    const QUrl lastUsedUrl = qnSettings->lastUsedConnection().url;
    if (lastUsedUrl.isValid() && lastUsedUrl != QnAppServerConnectionFactory::defaultUrl())
        return;

    QScopedPointer<LoginDialog> dialog(new LoginDialog(context(), widget()));
    dialog->setModal(true);
    while(true) {
        if(!dialog->exec())
            return;

        if(!context()->user())
            break; /* We weren't connected, so layouts cannot be saved. */

        QnWorkbenchState state;
        workbench()->submit(state);

        if(!closeAllLayouts(true))
            continue;
            
        QnWorkbenchStateHash states = qnSettings->userWorkbenchStates();
        states[context()->user()->getName()] = state;
        qnSettings->setUserWorkbenchStates(states);
        break;
    }
    menu()->trigger(Qn::ClearCameraSettingsAction);

    QnConnectionDataList connections = qnSettings->customConnections();

    QnConnectionData connectionData;
    connectionData.url = dialog->currentUrl();
    qnSettings->setLastUsedConnection(connectionData);

    // remove previous "Last used connection"
    connections.removeOne(QnConnectionDataList::defaultLastUsedName());

    QUrl clean_url(connectionData.url);
    clean_url.setPassword(QString());
    QnConnectionData selected = connections.getByName(dialog->currentName());
    if (selected.url == clean_url){
        connections.removeOne(selected.name);
        connections.prepend(selected);
    } else {
        // save "Last used connection"
        QnConnectionData last(connectionData);
        last.name = QnConnectionDataList::defaultLastUsedName();
        last.url.setPassword(QString());
        connections.prepend(last);
    }
    qnSettings->setCustomConnections(connections);

    //updateStoredConnections(connectionData);

    menu()->trigger(Qn::ReconnectAction, QnActionParameters().withArgument(Qn::ConnectInfoParameter, dialog->currentInfo()));
}

void QnWorkbenchActionHandler::at_reconnectAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    const QnConnectionData connectionData = qnSettings->lastUsedConnection();
    if (!connectionData.isValid()) 
        return;
    
    QnConnectInfoPtr connectionInfo = parameters.argument<QnConnectInfoPtr>(Qn::ConnectInfoParameter);
    if(connectionInfo.isNull()) {
        QnAppServerConnectionPtr connection = QnAppServerConnectionFactory::createConnection(connectionData.url);
        
        QScopedPointer<detail::QnConnectReplyProcessor> processor(new detail::QnConnectReplyProcessor());
        QScopedPointer<QEventLoop> loop(new QEventLoop());
        connect(processor.data(), SIGNAL(finished(int, const QByteArray &, const QnConnectInfoPtr &, int)), loop.data(), SLOT(quit()));
        connection->connectAsync(processor.data(), SLOT(at_replyReceived(int, const QByteArray &, const QnConnectInfoPtr &, int)));
        loop->exec();

        if(processor->status() != 0)
            return;

        connectionInfo = processor->info();
    }

    // TODO: maybe we need to check server-client compatibility here?

    QnAppServerConnectionFactory::setDefaultMediaProxyPort(connectionInfo->proxyPort);

    QnClientMessageProcessor::instance()->stop(); // TODO: blocks gui thread.
    QnSessionManager::instance()->stop();

    QnAppServerConnectionFactory::setCurrentVersion(connectionInfo->version);
    QnAppServerConnectionFactory::setDefaultUrl(connectionData.url);

    // repopulate the resource pool
    QnResource::stopCommandProc();
    QnResourceDiscoveryManager::instance().stop();

#ifndef STANDALONE_MODE
    static const char *appserverAddedPropertyName = "_qn_appserverAdded";
    if(!QnResourceDiscoveryManager::instance().property(appserverAddedPropertyName).toBool()) {
        QnResourceDiscoveryManager::instance().addDeviceServer(&QnAppServerResourceSearcher::instance());
        QnResourceDiscoveryManager::instance().setProperty(appserverAddedPropertyName, true);
    }
#endif

    // don't remove local resources
    const QnResourceList remoteResources = resourcePool()->getResourcesWithFlag(QnResource::remote);
    resourcePool()->setLayoutsUpdated(false);
    resourcePool()->removeResources(remoteResources);
    // Also remove layouts that were just added and have no 'remote' flag set. TODO: hack. 
    foreach(const QnLayoutResourcePtr &layout, resourcePool()->getResources().filtered<QnLayoutResource>())
        if(!(snapshotManager()->flags(layout) & Qn::ResourceIsLocal))
            resourcePool()->removeResource(layout);
    resourcePool()->setLayoutsUpdated(true);

    qnLicensePool->reset();

    QnSessionManager::instance()->start();
    QnClientMessageProcessor::instance()->run();

    QnAppServerResourceSearcher::instance().setShouldBeUsed(true);
    QnResourceDiscoveryManager::instance().start();
    QnResourceDiscoveryManager::instance().setReady(true);
    QnResource::startCommandProc();

    context()->setUserName(connectionData.url.userName());

    at_eventManager_connectionOpened();
}

void QnWorkbenchActionHandler::at_disconnectAction_triggered() {
    if(context()->user() && !closeAllLayouts(true))
        return;

    // TODO: Factor out common code from reconnect/disconnect/login actions.

    QnClientMessageProcessor::instance()->stop(); // TODO: blocks gui thread.
//    QnSessionManager::instance()->stop(); // omfg... logic sucks
    QnResource::stopCommandProc();
    QnResourceDiscoveryManager::instance().stop();

    // don't remove local resources
    const QnResourceList remoteResources = resourcePool()->getResourcesWithFlag(QnResource::remote);
    resourcePool()->setLayoutsUpdated(false);
    resourcePool()->removeResources(remoteResources);
    // Also remove layouts that were just added and have no 'remote' flag set. TODO: hack. 
    foreach(const QnLayoutResourcePtr &layout, resourcePool()->getResources().filtered<QnLayoutResource>())
        if(!(snapshotManager()->flags(layout) & Qn::ResourceIsLocal))
            resourcePool()->removeResource(layout);
    resourcePool()->setLayoutsUpdated(true);

    qnLicensePool->reset();

    QnAppServerConnectionFactory::setCurrentVersion(QLatin1String(""));
    // TODO: save workbench state on logout.
}

void QnWorkbenchActionHandler::at_editTagsAction_triggered() {
    QnResourcePtr resource = menu()->currentParameters(sender()).resource();
    if(!resource)
        return;

    QScopedPointer<TagsEditDialog> dialog(new TagsEditDialog(QStringList() << resource->getUniqueId(), widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_thumbnailsSearchAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnResourcePtr resource = parameters.resource();
    if(!resource)
        return;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodParameter);
    if(period.isEmpty())
        return;

    QnTimePeriodList periods = parameters.argument<QnTimePeriodList>(Qn::TimePeriodsParameter);

    /* List of possible time steps, in milliseconds. */
    const qint64 steps[] = {
        1000ll * 10,                    /* 10 seconds. */
        1000ll * 60,                    /* 1 minute. */
        1000ll * 60 * 5,                /* 5 minutes. */
        1000ll * 60 * 10,               /* 10 minutes. */
        1000ll * 60 * 60,               /* 1 hour. */
        1000ll * 60 * 60 * 3,           /* 3 hours. */
        1000ll * 60 * 60 * 6,           /* 6 hours. */
        1000ll * 60 * 60 * 24,          /* 1 day. */
        1000ll * 60 * 60 * 24 * 5,      /* 5 days. */
        1000ll * 60 * 60 * 24 * 30,     /* 30 days. */
        0,
    };
    const qint64 maxItems = 16; // TODO: take it from config?

    if(period.durationMs < steps[1]) {
        QMessageBox::warning(widget(), tr("Could not perform thumbnails search"), tr("Selected time period is too short to perform thumbnails search. Please select a longer period."), QMessageBox::Ok);
        return;
    }

    /* Find best time step. */
    qint64 step = 0;
    for(int i = 0; steps[i] > 0; i++) {
        if(period.durationMs < steps[i] * (maxItems - 2)) { /* -2 is here as we're going to snap period ends to closest step points. */
            step = steps[i];
            break;
        }
    }
    
    int itemCount = 0;
    if(step == 0) { 
        /* No luck? Calculate time step based on the maximal number of items. */
        itemCount = maxItems;

        step = period.durationMs / itemCount;
    } else {
        /* In this case we want to adjust the period. */

        if(resource->flags() & QnResource::utc) {
            QDateTime startDateTime = QDateTime::fromMSecsSinceEpoch(period.startTimeMs);
            QDateTime endDateTime = QDateTime::fromMSecsSinceEpoch(period.endTimeMs());
            const qint64 dayMSecs = 1000ll * 60 * 60 * 24;

            if(step < dayMSecs) {
                QTime base;

                int startMSecs = qFloor(QDateTime(startDateTime.date()).msecsTo(startDateTime), step);
                int endMSecs = qCeil(QDateTime(endDateTime.date()).msecsTo(endDateTime), step);

                startDateTime.setTime(QTime());
                startDateTime = startDateTime.addMSecs(startMSecs);

                endDateTime.setTime(QTime());
                endDateTime = endDateTime.addMSecs(endMSecs);
            } else {
                int stepDays = step / dayMSecs;

                startDateTime.setTime(QTime());
                startDateTime.setDate(QDate::fromJulianDay(qFloor(startDateTime.date().toJulianDay(), stepDays)));

                endDateTime.setTime(QTime());
                endDateTime.setDate(QDate::fromJulianDay(qCeil(endDateTime.date().toJulianDay(), stepDays)));
            }

            period = QnTimePeriod(startDateTime.toMSecsSinceEpoch(), endDateTime.toMSecsSinceEpoch() - startDateTime.toMSecsSinceEpoch());
        } else {
            qint64 startTime = qFloor(period.startTimeMs, step);
            qint64 endTime = qCeil(period.endTimeMs(), step);
            period = QnTimePeriod(startTime, endTime - startTime);
        }

        itemCount = period.durationMs / step;
    }

    /* Adjust for chunks. */
    if(!periods.isEmpty()) {
        qint64 startTime = periods[0].startTimeMs;

        while(startTime > period.startTimeMs + step / 2) {
            period.startTimeMs += step;
            period.durationMs -= step;
            itemCount--;
        }

        /*qint64 endTime = qnSyncTime->currentMSecsSinceEpoch();
        while(endTime < period.startTimeMs + period.durationMs) {
            period.durationMs -= step;
            itemCount--
        }*/
    }

    /* Calculate size of the resulting matrix. */
    const qreal desiredAspectRatio = 4.0 / 3.0;
    const int matrixWidth = qMax(1, qRound(std::sqrt(desiredAspectRatio * itemCount)));

    /* Construct and add a new layout. */
    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setGuid(QUuid::createUuid());
    layout->setName(tr("Thumbnail Search for %1").arg(resource->getName()));
    layout->setData(Qn::LayoutSyncStateRole, QVariant::fromValue<QnStreamSynchronizationState>(QnStreamSynchronizationState(true, DATETIME_NOW, 1.0))); // TODO: this does not belong here.
    if(context()->user())
        layout->setParentId(context()->user()->getId());

    qint64 time = period.startTimeMs;
    for(int i = 0; i < itemCount; i++) {
        QnLayoutItemData item;
        item.flags = Qn::Pinned;
        item.uuid = QUuid::createUuid();
        item.combinedGeometry = QRect(i % matrixWidth, i / matrixWidth, 1, 1);
        item.resource.id = resource->getId();
        item.resource.path = resource->getUniqueId();
        item.dataByRole[Qn::ItemPausedRole] = true;
        item.dataByRole[Qn::ItemSliderSelectionRole] = QVariant::fromValue<QnTimePeriod>(QnTimePeriod(time, step));
        item.dataByRole[Qn::ItemSliderWindowRole] = QVariant::fromValue<QnTimePeriod>(period);
        item.dataByRole[Qn::ItemTimeRole] = time;

        layout->addItem(item);

        time += step;
    }

    layout->setData(Qn::LayoutTimeLabelsRole, true);
    layout->setData(Qn::LayoutSyncStateRole, QVariant::fromValue<QnStreamSynchronizationState>(QnStreamSynchronizationState()));
    layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission));
    layout->setData(Qn::LayoutSearchStateRole, QVariant::fromValue<QnThumbnailsSearchState>(QnThumbnailsSearchState(period, step)));
    layout->setLocalRange(period);

    resourcePool()->addResource(layout);
    menu()->trigger(Qn::OpenSingleLayoutAction, layout);
}

void QnWorkbenchActionHandler::at_cameraSettingsAction_triggered() {
    QnVirtualCameraResourceList resources = menu()->currentParameters(sender()).resources().filtered<QnVirtualCameraResource>();
    bool newlyCreated = false;
    if(!cameraSettingsDialog()) {
        m_cameraSettingsDialog = new QnCameraSettingsDialog(widget());
        newlyCreated = true;

        connect(cameraSettingsDialog(), SIGNAL(buttonClicked(QDialogButtonBox::StandardButton)),    this, SLOT(at_cameraSettingsDialog_buttonClicked(QDialogButtonBox::StandardButton)));
        connect(cameraSettingsDialog(), SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)), this, SLOT(at_cameraSettingsDialog_scheduleExported(const QnVirtualCameraResourceList &)));
        connect(cameraSettingsDialog(), SIGNAL(rejected()),                                         this, SLOT(at_cameraSettingsDialog_rejected()));
        connect(cameraSettingsDialog(), SIGNAL(advancedSettingChanged()),                            this, SLOT(at_cameraSettingsAdvanced_changed()));
    }

    if(cameraSettingsDialog()->widget()->resources() != resources) {
        if(cameraSettingsDialog()->isVisible() && (
           cameraSettingsDialog()->widget()->hasDbChanges() || cameraSettingsDialog()->widget()->hasCameraChanges()))
        {
            QDialogButtonBox::StandardButton button = QnResourceListDialog::exec(
                widget(),
                QnResourceList(cameraSettingsDialog()->widget()->resources()),
                tr("Camera(s) not Saved"),
                tr("Save changes to the following %n camera(s)?", NULL, cameraSettingsDialog()->widget()->resources().size()),
                QDialogButtonBox::Yes | QDialogButtonBox::No
            );
            if(button == QDialogButtonBox::Yes)
                saveCameraSettingsFromDialog();
        }
    }
    cameraSettingsDialog()->widget()->setResources(resources);
    updateCameraSettingsEditibility();

    QRect oldGeometry = cameraSettingsDialog()->geometry();
    cameraSettingsDialog()->show();
    if(!newlyCreated)
        cameraSettingsDialog()->setGeometry(oldGeometry);

}

void QnWorkbenchActionHandler::at_clearCameraSettingsAction_triggered() {
    if(cameraSettingsDialog() && cameraSettingsDialog()->isVisible())
        menu()->trigger(Qn::OpenInCameraSettingsDialogAction, QnResourceList());
}

void QnWorkbenchActionHandler::at_cameraSettingsDialog_buttonClicked(QDialogButtonBox::StandardButton button) {
    switch(button) {
    case QDialogButtonBox::Ok:
        saveCameraSettingsFromDialog(true);
        break;
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

void QnWorkbenchActionHandler::at_cameraSettingsDialog_scheduleExported(const QnVirtualCameraResourceList &cameras){
    connection()->saveAsync(cameras, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
}

void QnWorkbenchActionHandler::at_cameraSettingsDialog_rejected() {
    cameraSettingsDialog()->widget()->updateFromResources();
}

void QnWorkbenchActionHandler::at_cameraSettingsAdvanced_changed() {
    if(!cameraSettingsDialog())
        return;

    bool hasCameraChanges = cameraSettingsDialog()->widget()->hasCameraChanges();

    if (!hasCameraChanges) {
        return;
    }

    QnVirtualCameraResourceList cameras = cameraSettingsDialog()->widget()->cameras();
    if(cameras.empty())
        return;

    cameraSettingsDialog()->widget()->submitToResources();
    saveAdvancedCameraSettingsAsync(cameras);
}

void QnWorkbenchActionHandler::at_selectionChangeAction_triggered() {
    if(!cameraSettingsDialog() || cameraSettingsDialog()->isHidden() || m_selectionUpdatePending)
        return;

    m_selectionUpdatePending = true;
    QTimer::singleShot(50, this, SLOT(updateCameraSettingsFromSelection()));
}

void QnWorkbenchActionHandler::at_serverAddCameraManuallyAction_triggered(){
    QnMediaServerResourceList resources = menu()->currentParameters(sender()).resources().filtered<QnMediaServerResource>();
    if(resources.size() != 1)
        return;

    QScopedPointer<QnCameraAdditionDialog> dialog(new QnCameraAdditionDialog(resources[0], widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_serverSettingsAction_triggered() {
    QnMediaServerResourceList resources = menu()->currentParameters(sender()).resources().filtered<QnMediaServerResource>();
    if(resources.size() != 1)
        return;

    QScopedPointer<QnServerSettingsDialog> dialog(new QnServerSettingsDialog(resources[0], widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    if(!dialog->exec())
        return;

    // TODO: move submitToResources here.
    connection()->saveAsync(resources[0], this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
}

void QnWorkbenchActionHandler::at_youtubeUploadAction_triggered() {
    QnResourcePtr resource = menu()->currentParameters(sender()).resource();
    if(resource.isNull())
        return;

    QScopedPointer<YouTubeUploadDialog> dialog(new YouTubeUploadDialog(context(), resource, widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_openInFolderAction_triggered() {
    QnResourcePtr resource = menu()->currentParameters(sender()).resource();
    if(resource.isNull())
        return;

    QnEnvironment::showInGraphicalShell(widget(), resource->getUrl());
}

void QnWorkbenchActionHandler::at_deleteFromDiskAction_triggered() {
    QSet<QnResourcePtr> resources = menu()->currentParameters(sender()).resources().toSet();

    QDialogButtonBox::StandardButton button = QnResourceListDialog::exec(
        widget(), 
        resources.toList(),
        tr("Delete Files"), 
        tr("Are you sure you want to permanently delete these %n file(s)?", 0, resources.size()), 
        QDialogButtonBox::Yes | QDialogButtonBox::No
    );
    if(button != QDialogButtonBox::Yes)
        return;
    
    QnFileProcessor::deleteLocalResources(resources.toList());
}

void QnWorkbenchActionHandler::at_removeLayoutItemAction_triggered() {
    QnLayoutItemIndexList items = menu()->currentParameters(sender()).layoutItems();

    if(items.size() > 1) {
        QDialogButtonBox::StandardButton button = QnResourceListDialog::exec(
            widget(),
            QnActionParameterTypes::resources(items),
            tr("Remove Items"),
            tr("Are you sure you want to remove these %n item(s) from layout?", 0, items.size()),
            QDialogButtonBox::Yes | QDialogButtonBox::No
        );
        if(button != QDialogButtonBox::Yes)
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
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnResourcePtr resource = parameters.resource();
    if(!resource)
        return;

    QString name = parameters.argument(Qn::NameParameter).toString();
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
        if(QnWorkbenchLayout::instance(layout))
            QnWorkbenchLayout::instance(layout)->setName(name); // TODO: hack

        if(!changed)
            snapshotManager()->save(layout, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
    } else {
        resource->setName(name);
        connection()->saveAsync(resource, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
    }
}

void QnWorkbenchActionHandler::at_removeFromServerAction_triggered() {
    QnResourceList resources = menu()->currentParameters(sender()).resources();
    
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
            QDialogButtonBox::Yes | QDialogButtonBox::No
        );
        okToDelete = button == QDialogButtonBox::Yes;
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
    user->setPermissions(Qn::GlobalLiveViewerPermissions);

    QScopedPointer<QnUserSettingsDialog> dialog(new QnUserSettingsDialog(context(), widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setEditorPermissions(accessController()->globalPermissions());
    dialog->setUser(user);
    dialog->setElementFlags(QnUserSettingsDialog::CurrentPassword, 0);
    setHelpTopic(dialog.data(), Qn::NewUser_Help);

    if(!dialog->exec())
        return;

    dialog->submitToResource();
    user->setGuid(QUuid::createUuid());

    connection()->saveAsync(user, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
}

void QnWorkbenchActionHandler::at_newUserLayoutAction_triggered() {
    QnUserResourcePtr user = menu()->currentParameters(sender()).resource().dynamicCast<QnUserResource>();
    if(user.isNull())
        return;

    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, widget()));
    dialog->setWindowTitle(tr("New Layout"));
    dialog->setText(tr("Enter the name of the layout to create:"));
    dialog->setName(newLayoutName(user));
    dialog->setWindowModality(Qt::ApplicationModal);
    if(!dialog->exec())
        return;

    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setGuid(QUuid::createUuid());
    layout->setName(dialog->name());
    layout->setParentId(user->getId());
    layout->setData(Qn::LayoutSyncStateRole, QVariant::fromValue<QnStreamSynchronizationState>(QnStreamSynchronizationState(true, DATETIME_NOW, 1.0))); // TODO: this does not belong here.
    resourcePool()->addResource(layout);

    snapshotManager()->save(layout, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));

    menu()->trigger(Qn::OpenSingleLayoutAction, QnActionParameters(layout));
}

void QnWorkbenchActionHandler::at_exitAction_triggered() {
    if(context()->user()) { // TODO: factor out
        QnWorkbenchState state;
        workbench()->submit(state);

        QnWorkbenchStateHash states = qnSettings->userWorkbenchStates();
        states[context()->user()->getName()] = state;
        qnSettings->setUserWorkbenchStates(states);
    }

    menu()->trigger(Qn::ClearCameraSettingsAction);
    if(closeAllLayouts(true))
        qApp->closeAllWindows();
}

void QnWorkbenchActionHandler::at_takeScreenshotAction_triggered() {
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();
    if(widgets.size() != 1)
        return;
    QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(widgets[0]); // TODO: check
    if (!widget) //e.g. server item
        return;

    QnResourceDisplay *display = widget->display();
    const QnResourceVideoLayout *layout = display->videoLayout();

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

    // TODO: move out, common code
    QString timeString;
    qint64 time = display->camDisplay()->getCurrentTime() / 1000;
    if(widget->resource()->flags() & QnResource::utc) {
        timeString = QDateTime::fromMSecsSinceEpoch(time).toString(QLatin1String("yyyy-MMM-dd_hh.mm.ss"));
    } else {
        timeString = QTime().addMSecs(time).toString(QLatin1String("hh.mm.ss"));
    }

    QString suggetion = replaceNonFileNameCharacters(widget->resource()->getName(), QLatin1Char('_')) + QLatin1Char('_') + timeString; 

    QSettings settings;
    settings.beginGroup(QLatin1String("screenshots"));

    QString previousDir = settings.value(QLatin1String("previousDir")).toString();
    if (!previousDir.length()){
        previousDir = qnSettings->mediaFolder();
    }

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
    QnUserResourcePtr user = menu()->currentParameters(sender()).resource().dynamicCast<QnUserResource>();
    if(!user)
        return;

    Qn::Permissions permissions = accessController()->permissions(user);
    if(!(permissions & Qn::ReadPermission))
        return;

    QScopedPointer<QnUserSettingsDialog> dialog(new QnUserSettingsDialog(context(), widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setWindowTitle(tr("User Settings"));
    setHelpTopic(dialog.data(), Qn::UserSettings_Help);

    QnUserSettingsDialog::ElementFlags zero(0);

    QnUserSettingsDialog::ElementFlags flags = 
        ((permissions & Qn::ReadPermission) ? QnUserSettingsDialog::Visible : zero) |
        ((permissions & Qn::WritePermission) ? QnUserSettingsDialog::Editable : zero);

    QnUserSettingsDialog::ElementFlags loginFlags = 
        ((permissions & Qn::ReadPermission) ? QnUserSettingsDialog::Visible : zero) |
        ((permissions & Qn::WriteNamePermission) ? QnUserSettingsDialog::Editable : zero);

    QnUserSettingsDialog::ElementFlags passwordFlags = 
        ((permissions & Qn::WritePasswordPermission) ? QnUserSettingsDialog::Visible : zero) | /* There is no point to display flag edit field if password cannot be changed. */
        ((permissions & Qn::WritePasswordPermission) ? QnUserSettingsDialog::Editable : zero);
    passwordFlags &= flags;

    QnUserSettingsDialog::ElementFlags accessRightsFlags = 
        ((permissions & Qn::ReadPermission) ? QnUserSettingsDialog::Visible : zero) |
        ((permissions & Qn::WriteAccessRightsPermission) ? QnUserSettingsDialog::Editable : zero);
    accessRightsFlags &= flags;

    dialog->setElementFlags(QnUserSettingsDialog::Login, loginFlags);
    dialog->setElementFlags(QnUserSettingsDialog::Password, passwordFlags);
    dialog->setElementFlags(QnUserSettingsDialog::AccessRights, accessRightsFlags);
    dialog->setEditorPermissions(accessController()->globalPermissions());
    

    // TODO #elric: This is a totally evil hack. Store password hash/salt in user.
    QString userPassword = qnSettings->lastUsedConnection().url.password();
    if(user == context()->user()) {
        dialog->setElementFlags(QnUserSettingsDialog::CurrentPassword, passwordFlags);
        dialog->setCurrentPassword(userPassword);
    } else {
        dialog->setElementFlags(QnUserSettingsDialog::CurrentPassword, 0);
    }

    dialog->setUser(user);
    if(!dialog->exec() || !dialog->hasChanges())
        return;

    if(permissions & Qn::SavePermission) {
        dialog->submitToResource();

        if (user->isAdmin() && userPassword != user->getPassword()) {
            QString message = tr("You have changed administrator password. Do not forget to change password on all connected mediaservers or they will stop working. Press 'Discard' to restore administrator password.");
            int button = QMessageBox::warning(widget(), tr("Changes are not applied"), message,
                                 QMessageBox::Ok, QMessageBox::Discard);
            if (button == QMessageBox::Discard) {
                user->setPassword(QString()); // TODO #gdm ask elric: why the hell we store empty strings?
                return; // we cannot change anything else for the Owner so we can return safely
            }
        }

        // TODO #gdm ask elric: should we restore empty user->password at at_resources_saved()?
        connection()->saveAsync(user, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));

        QString newPassword = user->getPassword();
        if(user == context()->user() && newPassword != userPassword) {
            /* Password was changed. Change it in global settings and hope for the best. */
            QnConnectionData data = qnSettings->lastUsedConnection();
            data.url.setPassword(newPassword);
            qnSettings->setLastUsedConnection(data);

            QnAppServerConnectionFactory::setDefaultUrl(data.url);
        }
    }
}

void QnWorkbenchActionHandler::at_exportLayoutAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnLayoutResourcePtr layout = workbench()->currentLayout()->resource();
    if (!layout)
        return;

    QnTimePeriod exportPeriod = parameters.argument<QnTimePeriod>(Qn::TimePeriodParameter);

    if(exportPeriod.durationMs * layout->getItems().size() > 1000 * 60 * 30) { // TODO: implement more precise estimation
        int button = QMessageBox::question(
            this->widget(),
            tr("Warning"),
            tr("You are about to export several video sequences with a total length exceeding 30 minutes. \n\
It may require over a gigabyte of HDD space, and, depending on your connection speed, may also take several minutes to complete.\n\
Do you want to continue?"),
               QMessageBox::Yes | QMessageBox::No
            );
        if(button == QMessageBox::No)
            return;
    }

    doAskNameAndExportLocalLayout(exportPeriod, layout, LayoutExport_Export);
}

bool QnWorkbenchActionHandler::validateItemTypes(QnLayoutResourcePtr layout)
{
    bool nonUtcExists = false;
    bool utcExists = false;
    bool imageExists = false;

    QnLayoutItemDataMap items = layout->getItems();
    for(QnLayoutItemDataMap::iterator itr = items.begin(); itr != items.end(); ++itr)
    {
        QnLayoutItemData& item = itr.value();
        QnResourcePtr layoutItemRes = qnResPool->getResourceByUniqId(item.resource.path);
        if (layoutItemRes) 
        {
            imageExists |= layoutItemRes->hasFlags(QnResource::still_image);
            bool isLocalItem = layoutItemRes->hasFlags(QnResource::local) || layoutItemRes->getUrl().startsWith(QLatin1String("layout://")); // layout item remove 'local' flag.
            if (isLocalItem && layoutItemRes->getStatus() == QnResource::Offline)
                continue; // skip unaccessible local resources because is not possible to check utc flag
            if (layoutItemRes->hasFlags(QnResource::utc))
                utcExists = true;
            else
                nonUtcExists = true;
        }
    }

    if (imageExists) {
        QMessageBox::critical(
            this->widget(), 
            tr("Can't create local layout"), 
            tr("Current layout contains image files. Images are not allowed for Multi-Video export."),
            QMessageBox::Ok
            );
        return false;
    }
    else if (nonUtcExists && utcExists) {
        QMessageBox::critical(
            this->widget(), 
            tr("Can't create local layout"), 
            tr("Current layout contains several cameras and several local files. You have to keep only cameras or only local files"), 
            QMessageBox::Ok
            );
        return false;
    }
    return true;
}

QString QnWorkbenchActionHandler::binaryFilterName(bool readOnly) const
{
    if (readOnly) {
        if (sizeof(char*) == 4)
            return tr("Executable %1 Media File (x86, read only) (*.exe)").arg(QLatin1String(QN_ORGANIZATION_NAME));
        else
            return tr("Executable %1 Media File (x64, read only) (*.exe)").arg(QLatin1String(QN_ORGANIZATION_NAME));
    }
    else {
        if (sizeof(char*) == 4)
            return tr("Executable %1 Media File (x86) (*.exe)").arg(QLatin1String(QN_ORGANIZATION_NAME));
        else
            return tr("Executable %1 Media File (x64) (*.exe)").arg(QLatin1String(QN_ORGANIZATION_NAME));
    }
}

void QnWorkbenchActionHandler::removeLayoutFromPool(QnLayoutResourcePtr existingLayout)
{
    QnLayoutItemDataMap items = existingLayout->getItems();
    for(QnLayoutItemDataMap::iterator itr = items.begin(); itr != items.end(); ++itr)
    {
        QnLayoutItemData& item = itr.value();
        QnResourcePtr layoutRes = qnResPool->getResourceByUniqId(item.resource.path);
        if (layoutRes)
            qnResPool->removeResource(layoutRes);
    }
    qnResPool->removeResource(existingLayout);
}

bool QnWorkbenchActionHandler::doAskNameAndExportLocalLayout(const QnTimePeriod& exportPeriod, QnLayoutResourcePtr layout, LayoutExportMode mode)
{
    if (!validateItemTypes(layout))
        return false;

    QString dialogName;
    if (mode == LayoutExport_LocalSaveAs)
        dialogName = tr("Save local layout As...");
    else if (mode == LayoutExport_Export)
        dialogName = tr("Export Layout As...");
    else
        return false; // not used

    QSettings settings;
    settings.beginGroup(QLatin1String("export"));
    QString previousDir = settings.value(QLatin1String("previousDir")).toString();
    if (!previousDir.length()){
        previousDir = qnSettings->mediaFolder();
    }

    QString suggestion = layout->getName();

    bool exportReadOnly = false;
    QString fileName;
    QString selectedExtension;
    QString filterSeparator(QLatin1String(";;"));
    QString mediaFileFilter = tr("Media File (*.nov)");
    QString mediaFileROFilter = tr("Media File (read only) (*.nov)");

    QString mediaFilter =
              QLatin1String(QN_ORGANIZATION_NAME) + QLatin1Char(' ') + mediaFileFilter
            + filterSeparator
            + QLatin1String(QN_ORGANIZATION_NAME) + QLatin1Char(' ') + mediaFileROFilter
            + filterSeparator
            + binaryFilterName(false)
            + filterSeparator
            + binaryFilterName(true);

    while (true) {
        QString selectedFilter;
        fileName = QFileDialog::getSaveFileName(
            this->widget(), 
            dialogName,
            previousDir + QDir::separator() + suggestion,
            mediaFilter,
            &selectedFilter,
            QFileDialog::DontUseNativeDialog
        );

        selectedExtension = selectedFilter.mid(selectedFilter.lastIndexOf(QLatin1Char('.')), 4);
        exportReadOnly = selectedFilter.contains(mediaFileROFilter) || selectedFilter.contains(binaryFilterName(true));
        if (fileName.isEmpty())
            return false;

        if (!fileName.toLower().endsWith(selectedExtension)) {
            fileName += selectedExtension;

            if (QFile::exists(fileName)) {
                QMessageBox::StandardButton button = QMessageBox::information(
                    this->widget(), 
                    tr("Save As"), 
                    tr("File '%1' already exists. Overwrite?").arg(QFileInfo(fileName).baseName()),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
                );

                if(button == QMessageBox::Cancel || button == QMessageBox::No)
                    return false;
            }
        }

        if (QFile::exists(fileName) && !QFile::remove(fileName)) {
            QMessageBox::critical(
                this->widget(), 
                tr("Can't overwrite file"), 
                tr("File '%1' is used by another process. Please try another name.").arg(QFileInfo(fileName).baseName()), 
                QMessageBox::Ok
            );
            continue;
        } 

        break;
    }
    settings.setValue(QLatin1String("previousDir"), QFileInfo(fileName).absolutePath());

    QnLayoutResourcePtr existingLayout = qnResPool->getResourceByUrl(QLatin1String("layout://") + fileName).dynamicCast<QnLayoutResource>();
    if (!existingLayout)
        existingLayout = qnResPool->getResourceByUrl(fileName).dynamicCast<QnLayoutResource>();
    if (existingLayout) 
        removeLayoutFromPool(existingLayout);

    saveLayoutToLocalFile(exportPeriod, layout, fileName, mode, exportReadOnly);

    return true;
}

void QnWorkbenchActionHandler::saveLayoutToLocalFile(const QnTimePeriod& exportPeriod, QnLayoutResourcePtr layout, const QString& layoutFileName, LayoutExportMode mode, bool exportReadOnly)
{
    if (m_exportedCamera)
    {
        QMessageBox::critical(
            this->widget(), 
            tr("Another export in progress"),
            tr("Another export in progress. Please wait"), 
            QMessageBox::Ok
        );

        return;
    }

    m_layoutExportMode = mode;
    m_layoutFileName = QnLayoutFileStorageResource::removeProtocolPrefix(layoutFileName); 
    QString fileName = m_layoutFileName;
    if (fileName == QnLayoutFileStorageResource::removeProtocolPrefix(layout->getUrl())) {
        // can not override opened layout. save to tmp file, then rename
        fileName += QLatin1String(".tmp");
    }

    QnProgressDialog *exportProgressDialog = new QnProgressDialog(this->widget());
    exportProgressDialog->setWindowTitle(tr("Exporting Layout"));
    exportProgressDialog->setMinimumDuration(1000);
    exportProgressDialog->setModal(true);
    exportProgressDialog->show();
    m_exportProgressDialog = exportProgressDialog;

    if (!m_layoutExportCamera)
        m_layoutExportCamera = new QnVideoCamera(QnMediaResourcePtr(0));
    m_exportedCamera = m_layoutExportCamera;
    connect(exportProgressDialog,   SIGNAL(canceled()),                 this,                   SLOT(at_cancelExport()));
    connect(exportProgressDialog,   SIGNAL(canceled()),                 exportProgressDialog,   SLOT(deleteLater()));
    connect(m_layoutExportCamera,   SIGNAL(exportProgress(int)),        exportProgressDialog,   SLOT(setValue(int)));
    connect(m_layoutExportCamera,   SIGNAL(exportFailed(QString)),      this,                   SLOT(at_layoutCamera_exportFailed(QString)));
    connect(m_layoutExportCamera,   SIGNAL(exportFinished(QString)),    this,                   SLOT(at_layoutCamera_exportFinished(QString)));

    if (m_layoutFileName.endsWith(QLatin1String(".exe")))
    {
        if (QnNovLauncher::createLaunchingFile(fileName) != 0)
        {
            at_layoutCamera_exportFailed(tr("File '%1' is used by another process. Please try another name.").arg(QFileInfo(fileName).baseName()));
            return;
        }
    }
    else {
        QFile::remove(fileName);
    }

    QString fullName = QLatin1String("layout://") + fileName;
    m_exportStorage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(fullName));
    m_exportStorage->setUrl(fullName);

    QIODevice* itemNamesIO = m_exportStorage->open(QLatin1String("item_names.txt"), QIODevice::WriteOnly);
    QTextStream itemNames(itemNamesIO);
    QList<qint64> itemTimeZones;

    m_layoutExportResources.clear();
    QSet<QString> uniqIdList;
    QnLayoutItemDataMap items = layout->getItems();

    for (QnLayoutItemDataMap::Iterator itr = items.begin(); itr != items.end(); ++itr)
    {
        (*itr).uuid = QUuid();
        QnResourcePtr resource = qnResPool->getResourceById((*itr).resource.id);
        if (resource == 0)
            resource = qnResPool->getResourceByUniqId((*itr).resource.path);
        if (resource)
        {
            itemNames << resource->getName() << QLatin1String("\n");
            QnMediaResourcePtr mediaRes = qSharedPointerDynamicCast<QnMediaResource>(resource);
            if (mediaRes) {
                (*itr).resource.id = 0;
                if (!uniqIdList.contains(mediaRes->getUniqueId())) {
                    m_layoutExportResources << mediaRes;
                    uniqIdList << mediaRes->getUniqueId();
                }
                itemTimeZones << context()->instance<QnWorkbenchServerTimeWatcher>()->utcOffset(mediaRes, Qn::InvalidUtcOffset);
            }
            else 
                itemTimeZones << Qn::InvalidUtcOffset;
        }
    }
    itemNames.flush();
    delete itemNamesIO;

    QIODevice* itemTimezonesIO = m_exportStorage->open(QLatin1String("item_timezones.txt"), QIODevice::WriteOnly);
    QTextStream itemTimeZonesStream(itemTimezonesIO);
    foreach(qint64 timeZone, itemTimeZones)
        itemTimeZonesStream << timeZone << QLatin1String("\n");
    itemTimeZonesStream.flush();
    delete itemTimezonesIO;

    QIODevice* device = m_exportStorage->open(QLatin1String("layout.pb"), QIODevice::WriteOnly);
    if (!device)
    {
        at_layoutCamera_exportFailed(tr("Could not create output file %1").arg(fileName));
        return;
    }

    QnApiPbSerializer serializer;
    QByteArray layoutData;
    serializer.serializeLayout(layout, layoutData);
    device->write(layoutData);
    delete device;

    device = m_exportStorage->open(QLatin1String("range.bin"), QIODevice::WriteOnly);
    device->write(exportPeriod.serialize());
    delete device;

    device = m_exportStorage->open(QLatin1String("misc.bin"), QIODevice::WriteOnly);
    quint32 flags = exportReadOnly ? 1 : 0;

    for (int i = 0; i < m_layoutExportResources.size(); ++i) {
        if (m_layoutExportResources[i]->hasFlags(QnResource::utc))
            flags |= 2; 
    }
    device->write((const char*) &flags, sizeof(flags));
    delete device;

    // If layout export create new guid. If layout just renamed (local save or local saveAs) keep guid
    QString uuid = (mode != LayoutExport_Export) ? layout->getGuid() : QUuid::createUuid().toString();
    device = m_exportStorage->open(QLatin1String("uuid.bin"), QIODevice::WriteOnly);
    device->write(uuid.toUtf8());
    delete device;

    for (int i = 0; i < m_layoutExportResources.size(); ++i)
    {
        QString uniqId = m_layoutExportResources[i]->getUniqueId();
        uniqId = uniqId.mid(uniqId.lastIndexOf(L'?') + 1);
        QnCachingTimePeriodLoader* loader = navigator()->loader(m_layoutExportResources[i]);
        if (loader) {
            QIODevice* device = m_exportStorage->open(QString(QLatin1String("chunk_%1.bin")).arg(QFileInfo(uniqId).baseName()) , QIODevice::WriteOnly);
            QnTimePeriodList periods = loader->periods(Qn::RecordingRole).intersected(exportPeriod);
            QByteArray data;
            periods.encode(data);
            device->write(data);
            delete device;
        }
    }

	// TODO: export progress dialog can be already deleted?
    exportProgressDialog->setRange(0, m_layoutExportResources.size() * 100);
    m_layoutExportCamera->setExportProgressOffset(-100);
    m_exportPeriod = exportPeriod;
    m_exportLayout = layout;
    at_layoutCamera_exportFinished(fileName);
}

void QnWorkbenchActionHandler::at_layout_exportFinished()
{
    disconnect(sender(), NULL, this, NULL); // TODO: not needed here.

    if(m_exportProgressDialog)
        m_exportProgressDialog.data()->deleteLater();
    if (!m_exportStorage)
        return; // race condition. Export just canceled from gui
    QString fileName = m_exportStorage->getUrl();
    if (fileName.endsWith(QLatin1String(".tmp")))
    {
        fileName.chop(4);
        m_exportStorage->renameFile(m_exportStorage->getUrl(), fileName);
        snapshotManager()->store(m_exportLayout);
    }
    else if (m_layoutExportMode == LayoutExport_LocalSaveAs) 
    {
        QString oldUrl = m_exportLayout->getUrl();
        QString newUrl = m_exportStorage->getUrl();

        QnLayoutItemDataMap items = m_exportLayout->getItems();
        for(QnLayoutItemDataMap::iterator itr = items.begin(); itr != items.end(); ++itr)
        {
            QnLayoutItemData& item = itr.value();
            QnAviResourcePtr aviRes = qnResPool->getResourceByUniqId(item.resource.path).dynamicCast<QnAviResource>();
            if (aviRes)
                qnResPool->updateUniqId(aviRes, QnLayoutResource::updateNovParent(newUrl, item.resource.path));
        }
        m_exportLayout->setUrl(newUrl);


        m_exportLayout->setName(QFileInfo(newUrl).fileName());

        QnLayoutFileStorageResourcePtr novStorage = m_exportStorage.dynamicCast<QnLayoutFileStorageResource>();
        if (novStorage)
            novStorage->switchToFile(oldUrl, newUrl, false);
        snapshotManager()->store(m_exportLayout);
    }
    else if (m_exportLayout && m_exportStorage) {
        QnLayoutResourcePtr layout =  QnResourceDirectoryBrowser::layoutFromFile(m_exportStorage->getUrl());
        if (!resourcePool()->getResourceByGuid(layout->getUniqueId())) {
            layout->setStatus(QnResource::Online);
            resourcePool()->addResource(layout);
        }
    }
    m_exportStorage.clear();
    m_exportedCamera = 0;

    if (m_layoutExportMode == LayoutExport_Export)
        QMessageBox::information(widget(), tr("Export finished"), tr("Export successfully finished"), QMessageBox::Ok);
}

void QnWorkbenchActionHandler::at_layoutCamera_exportFinished(QString fileName)
{
    Q_UNUSED(fileName)
    if (m_exportedMediaRes) 
    {
        int numberOfChannels = m_exportedMediaRes->getVideoLayout()->numberOfChannels();
        for (int i = 0; i < numberOfChannels; ++i)
        {
            if (m_motionFileBuffer[i])
            {
                m_motionFileBuffer[i]->close();
                
                QString uniqId = m_exportedMediaRes->getUniqueId();
                uniqId = QFileInfo(uniqId.mid(uniqId.indexOf(L'?')+1)).baseName(); // simplify name if export from existing layout
                QString motionFileName = QString(QLatin1String("motion%1_%2.bin")).arg(i).arg(uniqId);
                QIODevice* device = m_exportStorage->open(motionFileName , QIODevice::WriteOnly);
                device->write(m_motionFileBuffer[i]->buffer());
                device->close();
            }
            m_motionFileBuffer[i].clear();
        }
    }

    m_exportedMediaRes.clear();

    if (m_layoutExportResources.isEmpty()) {
        at_layout_exportFinished();
    } else {
        m_layoutExportCamera->setExportProgressOffset(m_layoutExportCamera->getExportProgressOffset() + 100);
        m_exportedMediaRes = m_layoutExportResources.dequeue();
        m_layoutExportCamera->setResource(m_exportedMediaRes);
        int numberOfChannels = m_exportedMediaRes->getVideoLayout()->numberOfChannels();
        for (int i = 0; i < numberOfChannels; ++i) {
            m_motionFileBuffer[i] = QSharedPointer<QBuffer>(new QBuffer());
            m_motionFileBuffer[i]->open(QIODevice::ReadWrite);
            m_layoutExportCamera->setMotionIODevice(m_motionFileBuffer[i], i);
        }

        QString uniqId = m_exportedMediaRes->getUniqueId();
        uniqId = uniqId.mid(uniqId.indexOf(L'?')+1); // simplify name if export from existing layout
        //QnStreamRecorder::Role role = m_exportStorage ? QnStreamRecorder::Role_FileExportWithEmptyContext : QnStreamRecorder::Role_FileExport;
        QnStreamRecorder::Role role = QnStreamRecorder::Role_FileExport;
        if (m_exportStorage && (m_exportedMediaRes->hasFlags(QnResource::utc)))
            role = QnStreamRecorder::Role_FileExportWithEmptyContext;
        m_layoutExportCamera->exportMediaPeriodToFile(m_exportPeriod.startTimeMs * 1000ll, (m_exportPeriod.startTimeMs + m_exportPeriod.durationMs) * 1000ll, uniqId, QLatin1String("mkv"), m_exportStorage, role);

        if(m_exportProgressDialog)
            m_exportProgressDialog.data()->setLabelText(tr("Exporting %1 to \"%2\"...").arg(m_exportedMediaRes->getUrl()).arg(m_layoutFileName));
    }
}

void QnWorkbenchActionHandler::at_layoutCamera_exportFailed(QString errorMessage) 
{
    at_cancelExport();
    if(m_exportProgressDialog)
        m_exportProgressDialog.data()->deleteLater();
    QMessageBox::warning(widget(), tr("Could not export layout"), errorMessage, QMessageBox::Ok);
}

void QnWorkbenchActionHandler::at_camera_settings_saved(int httpStatusCode, const QList<QPair<QString, bool> >& operationResult)
{
    QString error = httpStatusCode == 0? QString::fromLatin1("Possibly, appropriate camera's service is unavailable now"):
        QString::fromLatin1("Mediaserver returned the following error code : ") + httpStatusCode;

    QString failedParams;
    QList<QPair<QString, bool> >::ConstIterator it = operationResult.begin();
    for (; it != operationResult.end(); ++it)
    {
        if (!it->second) {
            QString formattedParam(QString::fromLatin1("Advanced->") + it->first.right(it->first.length() - 2));
            failedParams += QString::fromLatin1("\n");
            failedParams += formattedParam.replace(QString::fromLatin1("%%"), QString::fromLatin1("->"));
        }
    }

    if (!failedParams.isEmpty()) {
        QMessageBox::warning(
            widget(),
            tr("Currently parameters can't be saved."),
            tr("Failed to save the following parameters (%1):\n%2").arg(error, failedParams),
            1, 0);

        cameraSettingsDialog()->widget()->updateFromResources();
    }
}

void QnWorkbenchActionHandler::at_exportTimeSelectionAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnActionTargetProvider *provider = menu()->targetProvider();
    if(!provider)
        return;
    parameters.setItems(provider->currentTarget(Qn::SceneScope));

    QnMediaResourceWidget *widget = NULL;

    if(parameters.size() != 1) {
        if(parameters.size() == 0 && display()->widgets().size() == 1) {
            widget = dynamic_cast<QnMediaResourceWidget *>(display()->widgets().front());
        } else {
            QMessageBox::critical(
                this->widget(), 
                tr("Cannot export file"), 
                tr("Exactly one item must be selected for export, but %n item(s) are currently selected.", NULL, parameters.size()), 
                QMessageBox::Ok
            );
            return;
        }
    } else {
        widget = dynamic_cast<QnMediaResourceWidget *>(parameters.widget());
    }
    if(!widget)
        return;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodParameter);

    if(period.durationMs > 1000 * 60 * 30) { // TODO: implement more precise estimation
        int button = QMessageBox::question(
            this->widget(),
            tr("Warning"),
            tr("You are about to export a video sequence that is longer than 30 minutes. \n\
It may require over a gigabyte of HDD space, and, depending on your connection speed, may also take several minutes to complete.\n\
Do you want to continue?"),
            QMessageBox::Yes | QMessageBox::No
        );
        if(button == QMessageBox::No)
            return;
    }

    QnNetworkResourcePtr networkResource = widget->resource().dynamicCast<QnNetworkResource>();
    QnSecurityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSecurityCamResource>();

    QSettings settings;
    settings.beginGroup(QLatin1String("export"));
    QString previousDir = settings.value(QLatin1String("previousDir")).toString();
    if (!previousDir.length()){
        previousDir = qnSettings->mediaFolder();
    }

    QString dateFormat = cameraResource ? tr("dd-mmm-yyyy hh-mm-ss") : tr("hh-mm-ss");
        QString suggestion = networkResource ? networkResource->getPhysicalId() : QString();

    QString fileName;
    QString selectedExtension;
    QString selectedFilter;
    QString allowedFormatFilter = tr("AVI (*.avi);;Matroska (*.mkv);;");
    allowedFormatFilter += tr("AVI with Timestamps (Requires Transcoding)(*.avi);;Matroska  with Timestamps (Requires Transcoding)(*.mkv);;");
    allowedFormatFilter += binaryFilterName(false);
    while (true) {
        fileName = QFileDialog::getSaveFileName(
            this->widget(), 
            tr("Export Video As..."),
            previousDir + QDir::separator() + suggestion,
            allowedFormatFilter,
            &selectedFilter,
            QFileDialog::DontUseNativeDialog
        );
        selectedExtension = selectedFilter.mid(selectedFilter.lastIndexOf(QLatin1Char('.')), 4);

        if (fileName.isEmpty())
            return;

        if (!fileName.toLower().endsWith(selectedExtension)) {
            fileName += selectedExtension;

            if (QFile::exists(fileName)) {
                QMessageBox::StandardButton button = QMessageBox::information(
                    this->widget(), 
                    tr("Save As"), 
                    tr("File '%1' already exists. Overwrite?").arg(QFileInfo(fileName).baseName()),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
                );

                if(button == QMessageBox::Cancel || button == QMessageBox::No)
                    return;
            }
        }

        if (fileName.endsWith(QLatin1String(".avi")))
        {
            QnCachingTimePeriodLoader* loader = navigator()->loader(widget->resource());
            const QnArchiveStreamReader* archive = dynamic_cast<const QnArchiveStreamReader*> (widget->display()->dataProvider());
            if (loader && archive) 
            {
                QnTimePeriodList periods = loader->periods(Qn::RecordingRole).intersected(period);
                if (periods.size() > 1 && archive->getDPAudioLayout()->numberOfChannels() > 0)
                {
                    int result = QMessageBox::warning(
                        this->widget(), 
                        tr("AVI format is not recommended"), 
                        tr("AVI format is not recommended for camera with audio track there is some recording holes exists. Press 'Yes' to continue export or 'No' to select other format"),
                        QMessageBox::Yes | QMessageBox::No
                    );
                    if (result != QMessageBox::Yes)
                        continue;
                }
            }
        }

        if (QFile::exists(fileName) && !QFile::remove(fileName)) {
            QMessageBox::critical(
                this->widget(), 
                tr("Can't overwrite file"), 
                tr("File '%1' is used by another process. Please try another name.").arg(QFileInfo(fileName).baseName()), 
                QMessageBox::Ok
            );
            continue;
        }

        break;
    }
    settings.setValue(QLatin1String("previousDir"), QFileInfo(fileName).absolutePath());

    if (selectedExtension.toLower() == QLatin1String(".exe")) 
    {
        QnLayoutResourcePtr existingLayout = qnResPool->getResourceByUrl(QLatin1String("layout://") + fileName).dynamicCast<QnLayoutResource>();
        if (!existingLayout)
            existingLayout = qnResPool->getResourceByUrl(fileName).dynamicCast<QnLayoutResource>();
        if (existingLayout)
            removeLayoutFromPool(existingLayout);

        QnLayoutResourcePtr newLayout(new QnLayoutResource());

        QnLayoutItemData itemData = widget->item()->layout()->resource()->getItem(widget->item()->uuid());
        itemData.uuid = QUuid::createUuid();
        newLayout->addItem(itemData);
        saveLayoutToLocalFile(period, newLayout, fileName, LayoutExport_Export, false);
    }
    else {
        QnProgressDialog *exportProgressDialog = new QnProgressDialog(this->widget());
        exportProgressDialog->setWindowTitle(tr("Exporting Video"));
        exportProgressDialog->setLabelText(tr("Exporting to \"%1\"...").arg(fileName));
        exportProgressDialog->setRange(0, 100);
        exportProgressDialog->setMinimumDuration(1000);

        m_exportedCamera = widget->display()->camera();

        connect(exportProgressDialog,   SIGNAL(canceled()),                 this,                   SLOT(at_cancelExport()));
        connect(exportProgressDialog,   SIGNAL(canceled()),                 exportProgressDialog,   SLOT(deleteLater()));
        connect(m_exportedCamera,       SIGNAL(exportProgress(int)),        exportProgressDialog,   SLOT(setValue(int)));
        connect(m_exportedCamera,       SIGNAL(exportFailed(QString)),      exportProgressDialog,   SLOT(deleteLater()));
        connect(m_exportedCamera,       SIGNAL(exportFinished(QString)),    exportProgressDialog,   SLOT(deleteLater()));
        connect(m_exportedCamera,       SIGNAL(exportFailed(QString)),      this,                   SLOT(at_camera_exportFailed(QString)));
        connect(m_exportedCamera,       SIGNAL(exportFinished(QString)),    this,                   SLOT(at_camera_exportFinished(QString)));

        QnStreamRecorder::Role role = QnStreamRecorder::Role_FileExport;
        if (selectedFilter.contains(tr("with Timestamps")))
            role = QnStreamRecorder::Role_FileExportWithTime;
        QnMediaResourcePtr mediaRes = m_exportedCamera->getDevice().dynamicCast<QnMediaResource>();
        int timeOffset = 0;
        if(qnSettings->timeMode() == Qn::ServerTimeMode) {
            // time difference between client and server
            timeOffset = context()->instance<QnWorkbenchServerTimeWatcher>()->localOffset(mediaRes, 0);
        }
        qint64 serverTimeZone = context()->instance<QnWorkbenchServerTimeWatcher>()->utcOffset(mediaRes, Qn::InvalidUtcOffset);
        m_exportedCamera->exportMediaPeriodToFile(period.startTimeMs * 1000ll, (period.startTimeMs + period.durationMs) * 1000ll, fileName, selectedExtension.mid(1), 
                                                  QnStorageResourcePtr(), role, timeOffset, serverTimeZone);
        exportProgressDialog->exec();
    }
}


void QnWorkbenchActionHandler::at_camera_exportFinished(QString fileName) {
    disconnect(sender(), NULL, this, NULL);

    QnAviResourcePtr file(new QnAviResource(fileName));
    file->setStatus(QnResource::Online);
    resourcePool()->addResource(file);
    m_exportedCamera = 0;
    QMessageBox::information(widget(), tr("Export finished"), tr("Export successfully finished"), QMessageBox::Ok);
}

void QnWorkbenchActionHandler::at_cancelExport()
{
    QString exportFileName;
    if (m_exportedCamera) {
        exportFileName = m_exportedCamera->exportedFileName();
        m_exportedCamera->stopExport();
    }
    m_exportedCamera = 0;
    if (m_exportStorage)
        QFile::remove(QnLayoutFileStorageResource::removeProtocolPrefix(m_exportStorage->getUrl()));
    else if (!exportFileName.isEmpty())
        QFile::remove(exportFileName);
    m_exportStorage.clear();
    m_exportedMediaRes.clear();
}

void QnWorkbenchActionHandler::at_camera_exportFailed(QString errorMessage) {
    disconnect(sender(), NULL, this, NULL);

    if(QnVideoCamera *camera = dynamic_cast<QnVideoCamera *>(sender()))
        camera->stopExport();
    m_exportedCamera = 0;
    QMessageBox::warning(widget(), tr("Could not export video"), errorMessage, QMessageBox::Ok);
}

void QnWorkbenchActionHandler::at_setCurrentLayoutAspectRatio4x3Action_triggered() {
    workbench()->currentLayout()->resource()->setCellAspectRatio(4.0 / 3.0);
    action(Qn::SetCurrentLayoutAspectRatio4x3Action)->setChecked(true);
}

void QnWorkbenchActionHandler::at_setCurrentLayoutAspectRatio16x9Action_triggered() {
    workbench()->currentLayout()->resource()->setCellAspectRatio(16.0 / 9.0);
    action(Qn::SetCurrentLayoutAspectRatio16x9Action)->setChecked(true);
}

void QnWorkbenchActionHandler::at_setCurrentLayoutItemSpacing0Action_triggered() {
    workbench()->currentLayout()->resource()->setCellSpacing(QSizeF(0.0, 0.0));
    action(Qn::SetCurrentLayoutItemSpacing0Action)->setChecked(true);
}

void QnWorkbenchActionHandler::at_setCurrentLayoutItemSpacing10Action_triggered() {
    workbench()->currentLayout()->resource()->setCellSpacing(QSizeF(0.1, 0.1));
    action(Qn::SetCurrentLayoutItemSpacing10Action)->setChecked(true);
}

void QnWorkbenchActionHandler::at_setCurrentLayoutItemSpacing20Action_triggered() {
    workbench()->currentLayout()->resource()->setCellSpacing(QSizeF(0.2, 0.2));
    action(Qn::SetCurrentLayoutItemSpacing20Action)->setChecked(true);
}

void QnWorkbenchActionHandler::at_setCurrentLayoutItemSpacing30Action_triggered() {
    workbench()->currentLayout()->resource()->setCellSpacing(QSizeF(0.3, 0.3));
    action(Qn::SetCurrentLayoutItemSpacing30Action)->setChecked(true);
}

void QnWorkbenchActionHandler::at_rotate0Action_triggered(){
    rotateItems(0);
}

void QnWorkbenchActionHandler::at_rotate90Action_triggered(){
    rotateItems(90);
}

void QnWorkbenchActionHandler::at_rotate180Action_triggered(){
    rotateItems(180);
}

void QnWorkbenchActionHandler::at_rotate270Action_triggered(){
    rotateItems(270);
}

void QnWorkbenchActionHandler::at_radassAutoAction_triggered() {
    setItemsResolutionMode(Qn::AutoResolution);
}

void QnWorkbenchActionHandler::at_radassLowAction_triggered() {
    setItemsResolutionMode(Qn::LowResolution);
}

void QnWorkbenchActionHandler::at_radassHighAction_triggered() {
    setItemsResolutionMode(Qn::HighResolution);
}

void QnWorkbenchActionHandler::at_resources_saved(int status, const QByteArray& errorString, const QnResourceList &resources, int handle) {
    Q_UNUSED(handle);

    if(status == 0)
        return;

    QnLayoutResourceList layoutResources = resources.filtered<QnLayoutResource>();
    QnLayoutResourceList reopeningLayoutResources;
    foreach(const QnLayoutResourcePtr &layoutResource, layoutResources)
        if(snapshotManager()->isLocal(layoutResource) && !QnWorkbenchLayout::instance(layoutResource))
            reopeningLayoutResources.push_back(layoutResource);

    if(!reopeningLayoutResources.empty()) {
        int button = QnResourceListDialog::exec(
            widget(),
            resources,
            tr("Error"),
            tr("Could not save the following %n layout(s) to Enterprise Controller.", NULL, reopeningLayoutResources.size()),
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
            tr("Could not save the following %n items to Enterprise Controller.", NULL, resources.size()),
            tr("Error description: \n%1").arg(QLatin1String(errorString.data())),
            QDialogButtonBox::Ok
        );
    }
}

void QnWorkbenchActionHandler::at_resource_deleted(int status, const QByteArray &data, const QByteArray &errorString, int handle) {
    Q_UNUSED(handle);
    Q_UNUSED(data);

    if(status == 0)   
        return;

    QMessageBox::critical(widget(), tr(""), tr("Could not delete resource from Enterprise Controller. \n\nError description: '%2'").arg(QLatin1String(errorString.data())));
}

void QnWorkbenchActionHandler::at_resources_statusSaved(int status, const QByteArray &errorString, const QnResourceList &resources, const QList<int> &oldDisabledFlags) {
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
        resources[i]->setDisabled(oldDisabledFlags[i]);
}

void QnWorkbenchActionHandler::at_panicWatcher_panicModeChanged() {
    action(Qn::TogglePanicModeAction)->setChecked(context()->instance<QnWorkbenchPanicWatcher>()->isPanicMode());
}

void QnWorkbenchActionHandler::at_scheduleWatcher_scheduleEnabledChanged() {
    action(Qn::TogglePanicModeAction)->setEnabled(context()->instance<QnWorkbenchScheduleWatcher>()->isScheduleEnabled());
}

void QnWorkbenchActionHandler::at_togglePanicModeAction_toggled(bool checked) {
    QnMediaServerResourceList resources = resourcePool()->getResources().filtered<QnMediaServerResource>();

    foreach(QnMediaServerResourcePtr resource, resources) {
        if(resource->isPanicMode() != checked) {
            resource->setPanicMode(checked);
            connection()->saveAsync(resource, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
        }
    }
}

void QnWorkbenchActionHandler::at_updateWatcher_availableUpdateChanged() {
    QnUpdateInfoItem update = context()->instance<QnWorkbenchUpdateWatcher>()->availableUpdate();
    if(update.isNull())
        return;
    
    QMessageBox::information(
        widget(), 
        tr("Software Update is Available"), 
        tr("Version %1 is available for download at <a href=\"%2\">%2</a>.").arg(update.productVersion.toString()).arg(update.url.toString())
    );
}

void QnWorkbenchActionHandler::at_toggleTourAction_toggled(bool checked) {
    if(!checked) {
        m_tourTimer->stop();
        context()->workbench()->setItem(Qn::ZoomedRole, NULL);
    } else {
        m_tourTimer->start(qnSettings->tourCycleTime());
        at_tourTimer_timeout();
    }
}

struct ItemPositionCmp {
    bool operator()(QnWorkbenchItem *l, QnWorkbenchItem *r) const {
        QRect lg = l->geometry();
        QRect rg = r->geometry();
        return lg.y() < rg.y() || (lg.y() == rg.y() && lg.x() < rg.x());
    }
};

void QnWorkbenchActionHandler::at_tourTimer_timeout() {
    QList<QnWorkbenchItem *> items = context()->workbench()->currentLayout()->items().toList();
    qSort(items.begin(), items.end(), ItemPositionCmp());

    if(items.empty()) {
        action(Qn::ToggleTourModeAction)->setChecked(false);
        return;
    }

    QnWorkbenchItem *item = context()->workbench()->item(Qn::ZoomedRole);
    if(item) {
        item = items[(items.indexOf(item) + 1) % items.size()];
    } else {
        item = items[0];
    }
    context()->workbench()->setItem(Qn::ZoomedRole, item);
}

void QnWorkbenchActionHandler::at_workbench_itemChanged(Qn::ItemRole role) {
    Q_UNUSED(role)
    if(!workbench()->item(Qn::ZoomedRole))
        action(Qn::ToggleTourModeAction)->setChecked(false);
}

void QnWorkbenchActionHandler::at_whatsThisAction_triggered() {
    QWhatsThis::enterWhatsThisMode();
}


