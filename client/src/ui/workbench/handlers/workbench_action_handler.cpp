#include "workbench_action_handler.h"

#include <cassert>

#include <QtCore/QProcess>

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QFileDialog>
#include <QtGui/QImage>
#include <QtWidgets/QWhatsThis>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QCheckBox>
#include <QtGui/QImageWriter>

#include <utils/license_usage_helper.h>
#include <utils/app_server_image_cache.h>
#include <utils/app_server_notification_cache.h>
#include <utils/applauncher_utils.h>
#include <utils/local_file_cache.h>
#include <utils/common/environment.h>
#include <utils/common/delete_later.h>
#include <utils/common/mime_data.h>
#include <utils/common/event_processors.h>
#include <utils/common/string.h>
#include <utils/common/time.h>
#include <utils/common/email.h>
#include <utils/common/synctime.h>
#include <utils/math/math.h>

#include <api/session_manager.h>

#include <business/business_action_parameters.h>

#include <camera/resource_display.h>
#include <camera/cam_display.h>
#include <camera/caching_time_period_loader.h>

#include <client/client_connection_data.h>
#include <client/client_message_processor.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource/file_processor.h>

#include <device_plugins/server_camera/appserver.h>

#include <plugins/resources/archive/archive_stream_reader.h>
#include <plugins/resources/archive/avi_files/avi_resource.h>
#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <recording/time_period_list.h>

#include <redass/redass_controller.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_parameter_types.h>
#include <ui/actions/action_target_provider.h>

#include <ui/dialogs/about_dialog.h>
#include <ui/dialogs/login_dialog.h>
#include <ui/dialogs/server_settings_dialog.h>
#include <ui/dialogs/connection_testing_dialog.h>
#include <ui/dialogs/camera_settings_dialog.h>
#include <ui/dialogs/user_settings_dialog.h>
#include <ui/dialogs/resource_list_dialog.h>
#include <ui/dialogs/preferences_dialog.h>
#include <ui/dialogs/camera_addition_dialog.h>
#include <ui/dialogs/progress_dialog.h>
#include <ui/dialogs/business_rules_dialog.h>
#include <ui/dialogs/checkable_message_box.h>
#include <ui/dialogs/layout_settings_dialog.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/camera_diagnostics_dialog.h>
#include <ui/dialogs/message_box.h>
#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/dialogs/picture_settings_dialog.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/standard/graphics_message_box.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>

#include <ui/common/ui_resource_name.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/style/globals.h>
#include <ui/style/skin.h>

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

#include <ui/workbench/handlers/workbench_notifications_handler.h>
#include <ui/workbench/handlers/workbench_layouts_handler.h>

#include <ui/workbench/watchers/workbench_panic_watcher.h>
#include <ui/workbench/watchers/workbench_schedule_watcher.h>
#include <ui/workbench/watchers/workbench_update_watcher.h>
#include <ui/workbench/watchers/workbench_user_layout_count_watcher.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>
#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>

#include "version.h"

// TODO: #Elric remove this include
#include "../extensions/workbench_stream_synchronizer.h"

#include "core/resource/layout_item_data.h"
#include "ui/dialogs/adjust_video_dialog.h"
#include "ui/graphics/items/resource/resource_widget_renderer.h"
#include "ui/widgets/palette_widget.h"

namespace {
    const char* uploadingImageARPropertyName = "_qn_uploadingImageARPropertyName";
}

//!time that is given to process to exit. After that, appauncher (if present) will try to terminate it
static const quint32 PROCESS_TERMINATE_TIMEOUT = 15000;

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

void detail::QnResourceStatusReplyProcessor::at_replyReceived(int status, const QnResourceList &resources, int handle) {
    Q_UNUSED(handle);

    if(m_handler)
        m_handler.data()->at_resources_statusSaved(status, resources, m_oldDisabledFlags);

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

void detail::QnResourceReplyProcessor::at_replyReceived(int status, const QnResourceList &resources, int handle) {
    m_status = status;
    m_resources = resources;
    m_handle = handle;

    emit finished(status, resources, handle);
}


// -------------------------------------------------------------------------- //
// QnWorkbenchActionHandler
// -------------------------------------------------------------------------- //
QnWorkbenchActionHandler::QnWorkbenchActionHandler(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_selectionUpdatePending(false),
    m_selectionScope(Qn::SceneScope),
    m_tourTimer(new QTimer())
{
    connect(m_tourTimer,                                        SIGNAL(timeout()),                              this,   SLOT(at_tourTimer_timeout()));
    connect(context(),                                          SIGNAL(userChanged(const QnUserResourcePtr &)), this,   SLOT(at_context_userChanged(const QnUserResourcePtr &)), Qt::QueuedConnection);
    connect(context(),                                          SIGNAL(userChanged(const QnUserResourcePtr &)), this,   SLOT(updateCameraSettingsEditibility()));
    connect(QnClientMessageProcessor::instance(),               SIGNAL(connectionClosed()),                     this,   SLOT(at_messageProcessor_connectionClosed()));
    connect(QnClientMessageProcessor::instance(),               SIGNAL(connectionOpened()),                     this,   SLOT(at_messageProcessor_connectionOpened()));

    /* We're using queued connection here as modifying a field in its change notification handler may lead to problems. */
    connect(workbench(),                                        SIGNAL(layoutsChanged()),                       this,   SLOT(at_workbench_layoutsChanged()), Qt::QueuedConnection);
    connect(workbench(),                                        SIGNAL(itemChanged(Qn::ItemRole)),              this,   SLOT(at_workbench_itemChanged(Qn::ItemRole)));
    connect(workbench(),                                        SIGNAL(cellAspectRatioChanged()),               this,   SLOT(at_workbench_cellAspectRatioChanged()));
    connect(workbench(),                                        SIGNAL(cellSpacingChanged()),                   this,   SLOT(at_workbench_cellSpacingChanged()));
    connect(workbench(),                                        SIGNAL(currentLayoutChanged()),                 this,   SLOT(at_workbench_currentLayoutChanged()));

    connect(action(Qn::MainMenuAction),                         SIGNAL(triggered()),    this,   SLOT(at_mainMenuAction_triggered()));
    connect(action(Qn::OpenCurrentUserLayoutMenu),              SIGNAL(triggered()),    this,   SLOT(at_openCurrentUserLayoutMenuAction_triggered()));
    connect(action(Qn::CheckForUpdatesAction),                  SIGNAL(triggered()),    this,   SLOT(at_checkForUpdatesAction_triggered()));
    connect(action(Qn::ShowcaseAction),                         SIGNAL(triggered()),    this,   SLOT(at_showcaseAction_triggered()));
    connect(action(Qn::AboutAction),                            SIGNAL(triggered()),    this,   SLOT(at_aboutAction_triggered()));
    connect(action(Qn::OpenFileAction),                         SIGNAL(triggered()),    this,   SLOT(at_openFileAction_triggered()));
    connect(action(Qn::OpenLayoutAction),                       SIGNAL(triggered()),    this,   SLOT(at_openLayoutAction_triggered()));
    connect(action(Qn::OpenFolderAction),                       SIGNAL(triggered()),    this,   SLOT(at_openFolderAction_triggered()));
    connect(action(Qn::ConnectToServerAction),                  SIGNAL(triggered()),    this,   SLOT(at_connectToServerAction_triggered()));
    connect(action(Qn::PreferencesGeneralTabAction),            SIGNAL(triggered()),    this,   SLOT(at_preferencesGeneralTabAction_triggered()));
    connect(action(Qn::PreferencesLicensesTabAction),           SIGNAL(triggered()),    this,   SLOT(at_preferencesLicensesTabAction_triggered()));
    connect(action(Qn::PreferencesServerTabAction),             SIGNAL(triggered()),    this,   SLOT(at_preferencesServerTabAction_triggered()));
    connect(action(Qn::PreferencesNotificationTabAction),       SIGNAL(triggered()),    this,   SLOT(at_preferencesNotificationTabAction_triggered()));
    connect(action(Qn::ReconnectAction),                        SIGNAL(triggered()),    this,   SLOT(at_reconnectAction_triggered()));
    connect(action(Qn::DisconnectAction),                       SIGNAL(triggered()),    this,   SLOT(at_disconnectAction_triggered()));
    connect(action(Qn::BusinessEventsAction),                   SIGNAL(triggered()),    this,   SLOT(at_businessEventsAction_triggered()));
    connect(action(Qn::OpenBusinessRulesAction),                SIGNAL(triggered()),    this,   SLOT(at_openBusinessRulesAction_triggered()));
    connect(action(Qn::BusinessEventsLogAction),                SIGNAL(triggered()),    this,   SLOT(at_businessEventsLogAction_triggered()));
    connect(action(Qn::OpenBusinessLogAction),                  SIGNAL(triggered()),    this,   SLOT(at_openBusinessLogAction_triggered()));
    connect(action(Qn::CameraListAction),                       SIGNAL(triggered()),    this,   SLOT(at_cameraListAction_triggered()));
    connect(action(Qn::CameraListByServerAction),               SIGNAL(triggered()),    this,   SLOT(at_cameraListAction_triggered()));
    connect(action(Qn::WebClientAction),                        SIGNAL(triggered()),    this,   SLOT(at_webClientAction_triggered()));
    connect(action(Qn::NextLayoutAction),                       SIGNAL(triggered()),    this,   SLOT(at_nextLayoutAction_triggered()));
    connect(action(Qn::PreviousLayoutAction),                   SIGNAL(triggered()),    this,   SLOT(at_previousLayoutAction_triggered()));
    connect(action(Qn::OpenInLayoutAction),                     SIGNAL(triggered()),    this,   SLOT(at_openInLayoutAction_triggered()));
    connect(action(Qn::OpenInCurrentLayoutAction),              SIGNAL(triggered()),    this,   SLOT(at_openInCurrentLayoutAction_triggered()));
    connect(action(Qn::OpenInNewLayoutAction),                  SIGNAL(triggered()),    this,   SLOT(at_openInNewLayoutAction_triggered()));
    connect(action(Qn::OpenInNewWindowAction),                  SIGNAL(triggered()),    this,   SLOT(at_openInNewWindowAction_triggered()));
    connect(action(Qn::OpenSingleLayoutAction),                 SIGNAL(triggered()),    this,   SLOT(at_openLayoutsAction_triggered()));
    connect(action(Qn::OpenMultipleLayoutsAction),              SIGNAL(triggered()),    this,   SLOT(at_openLayoutsAction_triggered()));
    connect(action(Qn::OpenAnyNumberOfLayoutsAction),           SIGNAL(triggered()),    this,   SLOT(at_openLayoutsAction_triggered()));
    connect(action(Qn::OpenLayoutsInNewWindowAction),           SIGNAL(triggered()),    this,   SLOT(at_openLayoutsInNewWindowAction_triggered()));
    connect(action(Qn::OpenCurrentLayoutInNewWindowAction),     SIGNAL(triggered()),    this,   SLOT(at_openCurrentLayoutInNewWindowAction_triggered()));
    connect(action(Qn::OpenNewTabAction),                       SIGNAL(triggered()),    this,   SLOT(at_openNewTabAction_triggered()));
    connect(action(Qn::OpenNewWindowAction),                    SIGNAL(triggered()),    this,   SLOT(at_openNewWindowAction_triggered()));
    connect(action(Qn::UserSettingsAction),                     SIGNAL(triggered()),    this,   SLOT(at_userSettingsAction_triggered()));
    connect(action(Qn::CameraSettingsAction),                   SIGNAL(triggered()),    this,   SLOT(at_cameraSettingsAction_triggered()));
    connect(action(Qn::PictureSettingsAction),                  SIGNAL(triggered()),    this,   SLOT(at_pictureSettingsAction_triggered()));
    connect(action(Qn::CameraIssuesAction),                     SIGNAL(triggered()),    this,   SLOT(at_cameraIssuesAction_triggered()));
    connect(action(Qn::CameraBusinessRulesAction),              SIGNAL(triggered()),    this,   SLOT(at_cameraBusinessRulesAction_triggered()));
    connect(action(Qn::CameraDiagnosticsAction),                SIGNAL(triggered()),    this,   SLOT(at_cameraDiagnosticsAction_triggered()));
    connect(action(Qn::LayoutSettingsAction),                   SIGNAL(triggered()),    this,   SLOT(at_layoutSettingsAction_triggered()));
    connect(action(Qn::CurrentLayoutSettingsAction),            SIGNAL(triggered()),    this,   SLOT(at_currentLayoutSettingsAction_triggered()));
    connect(action(Qn::OpenInCameraSettingsDialogAction),       SIGNAL(triggered()),    this,   SLOT(at_cameraSettingsAction_triggered()));
    connect(action(Qn::ClearCameraSettingsAction),              SIGNAL(triggered()),    this,   SLOT(at_clearCameraSettingsAction_triggered()));
    connect(action(Qn::SelectionChangeAction),                  SIGNAL(triggered()),    this,   SLOT(at_selectionChangeAction_triggered()));
    connect(action(Qn::ServerAddCameraManuallyAction),          SIGNAL(triggered()),    this,   SLOT(at_serverAddCameraManuallyAction_triggered()));
    connect(action(Qn::ServerSettingsAction),                   SIGNAL(triggered()),    this,   SLOT(at_serverSettingsAction_triggered()));
    connect(action(Qn::PingAction),                             SIGNAL(triggered()),    this,   SLOT(at_pingAction_triggered()));
    connect(action(Qn::ServerLogsAction),                       SIGNAL(triggered()),    this,   SLOT(at_serverLogsAction_triggered()));
    connect(action(Qn::ServerIssuesAction),                     SIGNAL(triggered()),    this,   SLOT(at_serverIssuesAction_triggered()));
    connect(action(Qn::YouTubeUploadAction),                    SIGNAL(triggered()),    this,   SLOT(at_youtubeUploadAction_triggered()));
    connect(action(Qn::OpenInFolderAction),                     SIGNAL(triggered()),    this,   SLOT(at_openInFolderAction_triggered()));
    connect(action(Qn::DeleteFromDiskAction),                   SIGNAL(triggered()),    this,   SLOT(at_deleteFromDiskAction_triggered()));
    connect(action(Qn::RemoveLayoutItemAction),                 SIGNAL(triggered()),    this,   SLOT(at_removeLayoutItemAction_triggered()));
    connect(action(Qn::RemoveFromServerAction),                 SIGNAL(triggered()),    this,   SLOT(at_removeFromServerAction_triggered()));
    connect(action(Qn::NewUserAction),                          SIGNAL(triggered()),    this,   SLOT(at_newUserAction_triggered()));
    connect(action(Qn::RenameAction),                           SIGNAL(triggered()),    this,   SLOT(at_renameAction_triggered()));
    connect(action(Qn::DropResourcesAction),                    SIGNAL(triggered()),    this,   SLOT(at_dropResourcesAction_triggered()));
    connect(action(Qn::DelayedDropResourcesAction),             SIGNAL(triggered()),    this,   SLOT(at_delayedDropResourcesAction_triggered()));
    connect(action(Qn::InstantDropResourcesAction),             SIGNAL(triggered()),    this,   SLOT(at_instantDropResourcesAction_triggered()));
    connect(action(Qn::DropResourcesIntoNewLayoutAction),       SIGNAL(triggered()),    this,   SLOT(at_dropResourcesIntoNewLayoutAction_triggered()));
    connect(action(Qn::MoveCameraAction),                       SIGNAL(triggered()),    this,   SLOT(at_moveCameraAction_triggered()));
    connect(action(Qn::AdjustVideoAction),                      SIGNAL(triggered()),    this,   SLOT(at_adjustVideoAction_triggered()));
    connect(action(Qn::ExitAction),                             SIGNAL(triggered()),    this,   SLOT(at_exitAction_triggered()));
    connect(action(Qn::ThumbnailsSearchAction),                 SIGNAL(triggered()),    this,   SLOT(at_thumbnailsSearchAction_triggered()));
    connect(action(Qn::SetCurrentLayoutAspectRatio4x3Action),   SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutAspectRatio4x3Action_triggered()));
    connect(action(Qn::SetCurrentLayoutAspectRatio16x9Action),  SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutAspectRatio16x9Action_triggered()));
    connect(action(Qn::SetCurrentLayoutItemSpacing0Action),     SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutItemSpacing0Action_triggered()));
    connect(action(Qn::SetCurrentLayoutItemSpacing10Action),    SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutItemSpacing10Action_triggered()));
    connect(action(Qn::SetCurrentLayoutItemSpacing20Action),    SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutItemSpacing20Action_triggered()));
    connect(action(Qn::SetCurrentLayoutItemSpacing30Action),    SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutItemSpacing30Action_triggered()));
    connect(action(Qn::CreateZoomWindowAction),                 SIGNAL(triggered()),    this,   SLOT(at_createZoomWindowAction_triggered()));
    connect(action(Qn::Rotate0Action),                          SIGNAL(triggered()),    this,   SLOT(at_rotate0Action_triggered()));
    connect(action(Qn::Rotate90Action),                         SIGNAL(triggered()),    this,   SLOT(at_rotate90Action_triggered()));
    connect(action(Qn::Rotate180Action),                        SIGNAL(triggered()),    this,   SLOT(at_rotate180Action_triggered()));
    connect(action(Qn::Rotate270Action),                        SIGNAL(triggered()),    this,   SLOT(at_rotate270Action_triggered()));
    connect(action(Qn::RadassAutoAction),                       SIGNAL(triggered()),    this,   SLOT(at_radassAutoAction_triggered()));
    connect(action(Qn::RadassLowAction),                        SIGNAL(triggered()),    this,   SLOT(at_radassLowAction_triggered()));
    connect(action(Qn::RadassHighAction),                       SIGNAL(triggered()),    this,   SLOT(at_radassHighAction_triggered()));
    connect(action(Qn::SetAsBackgroundAction),                  SIGNAL(triggered()),    this,   SLOT(at_setAsBackgroundAction_triggered()));
    connect(action(Qn::WhatsThisAction),                        SIGNAL(triggered()),    this,   SLOT(at_whatsThisAction_triggered()));
    connect(action(Qn::EscapeHotkeyAction),                     SIGNAL(triggered()),    this,   SLOT(at_escapeHotkeyAction_triggered()));
    connect(action(Qn::ClearCacheAction),                       SIGNAL(triggered()),    this,   SLOT(at_clearCacheAction_triggered()));
    connect(action(Qn::MessageBoxAction),                       SIGNAL(triggered()),    this,   SLOT(at_messageBoxAction_triggered()));
    connect(action(Qn::BrowseUrlAction),                        SIGNAL(triggered()),    this,   SLOT(at_browseUrlAction_triggered()));
    connect(action(Qn::VersionMismatchMessageAction),           SIGNAL(triggered()),    this,   SLOT(at_versionMismatchMessageAction_triggered()));
    connect(action(Qn::BetaVersionMessageAction),               SIGNAL(triggered()),    this,   SLOT(at_betaVersionMessageAction_triggered()));
    connect(action(Qn::QueueAppRestartAction),                  SIGNAL(triggered()),    this,   SLOT(at_queueAppRestartAction_triggered()), Qt::QueuedConnection);

    connect(action(Qn::TogglePanicModeAction),                  SIGNAL(toggled(bool)),  this,   SLOT(at_togglePanicModeAction_toggled(bool)));
    connect(action(Qn::ToggleTourModeAction),                   SIGNAL(toggled(bool)),  this,   SLOT(at_toggleTourAction_toggled(bool)));
    connect(context()->instance<QnWorkbenchPanicWatcher>(),     SIGNAL(panicModeChanged()), this, SLOT(at_panicWatcher_panicModeChanged()));
    connect(context()->instance<QnWorkbenchScheduleWatcher>(),  SIGNAL(scheduleEnabledChanged()), this, SLOT(at_scheduleWatcher_scheduleEnabledChanged()));
    connect(context()->instance<QnWorkbenchUpdateWatcher>(),    SIGNAL(availableUpdateChanged()), this, SLOT(at_updateWatcher_availableUpdateChanged()));
    connect(context()->instance<QnWorkbenchVersionMismatchWatcher>(), SIGNAL(mismatchDataChanged()), this, SLOT(at_versionMismatchWatcher_mismatchDataChanged()));

    context()->instance<QnAppServerNotificationCache>();

    /* Run handlers that update state. */
    at_messageProcessor_connectionClosed();
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

    if (businessRulesDialog())
        delete businessRulesDialog();

    if (businessEventsLogDialog())
        delete businessEventsLogDialog();

    if (cameraAdditionDialog())
        delete cameraAdditionDialog();

    if (loginDialog())
        delete loginDialog();
}

QnAppServerConnectionPtr QnWorkbenchActionHandler::connection() const {
    return QnAppServerConnectionFactory::createConnection();
}

bool QnWorkbenchActionHandler::canAutoDelete(const QnResourcePtr &resource) const {
    QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
    if(!layoutResource)
        return false;

    return snapshotManager()->flags(layoutResource) == Qn::ResourceIsLocal; /* Local, not changed and not being saved. */
}

void QnWorkbenchActionHandler::addToLayout(const QnLayoutResourcePtr &layout, const QnResourcePtr &resource, const AddToLayoutParams &params) const {

    if (qnSettings->lightMode() & Qn::LightModeSingleItem) {
        while (!layout->getItems().isEmpty())
            layout->removeItem(*(layout->getItems().begin()));
    }

    int maxItems = (qnSettings->lightMode() & Qn::LightModeSingleItem)
            ? 1
            : qnSettings->maxSceneVideoItems();

    if (layout->getItems().size() >= maxItems)
        return;

    {
        //TODO: #GDM refactor duplicated code
        bool isServer = resource->hasFlags(QnResource::server);
        bool isMediaResource = resource->hasFlags(QnResource::media);
        bool isLocalResource = resource->hasFlags(QnResource::url | QnResource::local | QnResource::media)
                && !resource->getUrl().startsWith(QnLayoutFileStorageResource::layoutPrefix());
        bool isExportedLayout = layout->hasFlags(QnResource::url | QnResource::local | QnResource::layout);

        bool allowed = isServer || isMediaResource;
        bool forbidden = isExportedLayout && (isServer || isLocalResource);
        if(!allowed || forbidden)
            return;
    }

    QnLayoutItemData data;
    data.resource.id = resource->getId();
    data.resource.path = resource->getUniqueId();
    data.uuid = QUuid::createUuid();
    data.flags = Qn::PendingGeometryAdjustment;
    data.zoomRect = params.zoomWindow;
    data.zoomTargetUuid = params.zoomUuid;
    data.rotation = params.rotation;
    data.contrastParams = params.contrastParams;
    data.dewarpingParams = params.dewarpingParams;
    data.dataByRole[Qn::ItemTimeRole] = params.time;
    if(params.frameColor.isValid())
        data.dataByRole[Qn::ItemFrameColorRole] = params.frameColor;
    if(params.usePosition) {
        data.combinedGeometry = QRectF(params.position, params.position); /* Desired position is encoded into a valid rect. */
    } else {
        data.combinedGeometry = QRectF(QPointF(0.5, 0.5), QPointF(-0.5, -0.5)); /* The fact that any position is OK is encoded into an invalid rect. */
    }
    layout->addItem(data);
}

void QnWorkbenchActionHandler::addToLayout(const QnLayoutResourcePtr &layout, const QnResourceList &resources, const AddToLayoutParams &params) const {
    foreach(const QnResourcePtr &resource, resources)
        addToLayout(layout, resource, params);
}

void QnWorkbenchActionHandler::addToLayout(const QnLayoutResourcePtr &layout, const QList<QnMediaResourcePtr>& resources, const AddToLayoutParams &params) const {
    foreach(const QnMediaResourcePtr &resource, resources)
        addToLayout(layout, resource->toResourcePtr(), params);
}

void QnWorkbenchActionHandler::addToLayout(const QnLayoutResourcePtr &layout, const QList<QString> &files, const AddToLayoutParams &params) const {
    addToLayout(layout, addToResourcePool(files), params);
}

QnResourceList QnWorkbenchActionHandler::addToResourcePool(const QList<QString> &files) const {
    return QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(files));
}

QnResourceList QnWorkbenchActionHandler::addToResourcePool(const QString &file) const {
    return QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(file));
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
    arguments << QLatin1String("--no-version-mismatch-check");

    if (context()->user()) {
        arguments << QLatin1String("--auth");
        arguments << QLatin1String(qnSettings->lastUsedConnection().url.toEncoded());
    }

    /* For now, simply open it at another screen. Don't account for 3+ monitor setups. */
    if(mainWindow()) {
        int screen = qApp->desktop()->screenNumber(mainWindow());
        screen = (screen + 1) % qApp->desktop()->screenCount();

        arguments << QLatin1String("--screen");
        arguments << QString::number(screen);
    }

    if (qnSettings->isDevMode())
        arguments << QLatin1String("--dev-mode-key=razrazraz");

    qDebug() << "Starting new instance with args" << arguments;

    QProcess::startDetached(qApp->applicationFilePath(), arguments);
}

void QnWorkbenchActionHandler::saveCameraSettingsFromDialog(bool checkControls) {
    if(!cameraSettingsDialog())
        return;

    bool hasDbChanges = cameraSettingsDialog()->widget()->hasDbChanges();
    bool hasCameraChanges = cameraSettingsDialog()->widget()->hasCameraChanges();

    if (checkControls && cameraSettingsDialog()->widget()->hasScheduleControlsChanges()){
        // TODO: #Elric #TR remove first space.
        QString message = tr(" Recording changes have not been saved. Pick desired Recording Type, FPS, and Quality and mark the changes on the schedule.");
        int button = QMessageBox::warning(cameraSettingsDialog(), tr("Changes are not applied"), message, QMessageBox::Retry, QMessageBox::Ignore);
        if (button == QMessageBox::Retry) {
            cameraSettingsDialog()->ignoreAcceptOnce();
            return;
        } else {
            cameraSettingsDialog()->widget()->clearScheduleControlsChanges();
        }
    } else if (checkControls && cameraSettingsDialog()->widget()->hasMotionControlsChanges()){
        QString message = tr("Actual motion sensitivity was not changed. To change motion sensitivity draw rectangles on the image.");
        int button = QMessageBox::warning(cameraSettingsDialog(), tr("Changes are not applied"), message, QMessageBox::Retry, QMessageBox::Ignore);
        if (button == QMessageBox::Retry){
            cameraSettingsDialog()->ignoreAcceptOnce();
            return;
        } else {
            cameraSettingsDialog()->widget()->clearMotionControlsChanges();
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

    //checking if showing Licenses limit exceeded is appropriate
    if( cameraSettingsDialog()->widget()->licensedParametersModified() )
    {
        QnLicenseUsageHelper helper(cameras, cameraSettingsDialog()->widget()->isScheduleEnabled());
        if (!helper.isValid())
        {
            QString message = tr("Licenses limit exceeded. The changes will be saved, but will not take effect.");
            QMessageBox::warning(cameraSettingsDialog(), tr("Could not apply changes"), message);
            cameraSettingsDialog()->widget()->setScheduleEnabled(false);
        }
    }

    /* Submit and save it. */
    cameraSettingsDialog()->widget()->submitToResources();

    if (hasDbChanges) {
        connection()->saveAsync(cameras, this, SLOT(at_resources_saved(int, const QnResourceList &, int)));
        foreach(const QnResourcePtr &camera, cameras) {
            connection()->saveAsync(camera->getId(), camera->getProperties());
        }
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
        QString error = lit("Connection refused"); // #TR #Elric

        QString failedParams;
        QList< QPair< QString, QVariant> >::ConstIterator it =
            cameraSettingsDialog()->widget()->getModifiedAdvancedParams().begin();
        for (; it != cameraSettingsDialog()->widget()->getModifiedAdvancedParams().end(); ++it)
        {
            QString formattedParam(it->first.right(it->first.length() - 2));
            failedParams += lit("\n"); // #TR #Elric
            failedParams += formattedParam.replace(lit("%%"), lit("->")); // #TR? #Elric
        }

        if (!failedParams.isEmpty()) {
            QMessageBox::warning(
                mainWindow(),
                tr("Could not save parameters"),
                tr("Failed to save the following parameters (%1):\n%2").arg(error, failedParams)
            );

            cameraSettingsDialog()->widget()->updateFromResources();
        }

        return;
    }

    // TODO: #Elric method called even if nothing has changed
    // TODO: #Elric result slot at_camera_settings_saved() is not called
    serverConnectionPtr->setParamsAsync(cameraPtr, cameraSettingsDialog()->widget()->getModifiedAdvancedParams(),
        this, SLOT(at_camera_settings_saved(int, const QList<QPair<QString, bool> >&)) );
}

void QnWorkbenchActionHandler::rotateItems(int degrees){
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();
    if(!widgets.empty()) {
        foreach(const QnResourceWidget *widget, widgets)
            widget->item()->setRotation(degrees);
    }
}

void QnWorkbenchActionHandler::setResolutionMode(Qn::ResolutionMode resolutionMode) {
    qnRedAssController->setMode(resolutionMode);
}

QnWorkbenchNotificationsHandler* QnWorkbenchActionHandler::notificationsHandler() const {
    return context()->instance<QnWorkbenchNotificationsHandler>();
}

QnCameraSettingsDialog *QnWorkbenchActionHandler::cameraSettingsDialog() const {
    return m_cameraSettingsDialog.data();
}

QnBusinessRulesDialog *QnWorkbenchActionHandler::businessRulesDialog() const {
    return m_businessRulesDialog.data();
}

QnEventLogDialog *QnWorkbenchActionHandler::businessEventsLogDialog() const {
    return m_businessEventsLogDialog.data();
}

QnCameraListDialog *QnWorkbenchActionHandler::cameraListDialog() const {
    return m_cameraListDialog.data();
}

QnCameraAdditionDialog *QnWorkbenchActionHandler::cameraAdditionDialog() const {
    return m_cameraAdditionDialog.data();
}

QnLoginDialog *QnWorkbenchActionHandler::loginDialog() const {
    return m_loginDialog.data();
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

    menu()->trigger(Qn::OpenInCameraSettingsDialogAction, provider->currentParameters(scope));
}

void QnWorkbenchActionHandler::submitDelayedDrops() {
    if(!context()->user())
        return;

    if (!context()->workbench()->currentLayout()->resource())
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

void QnWorkbenchActionHandler::submitInstantDrop() {

    if (QnResourceDiscoveryManager::instance()->state() == QnResourceDiscoveryManager::initialSearch) {
        // local resources is not ready yet
        QTimer::singleShot(100, this, SLOT(submitInstantDrop()));
        return;
    }

    foreach(const QnMimeData &data, m_instantDrops) {
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
    m_instantDrops.clear();
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

    // we should not change state when using "Open in New Window"
    if (m_delayedDrops.isEmpty()) {
        QnWorkbenchState state = qnSettings->userWorkbenchStates().value(user->getName());
        workbench()->update(state);

        /* Delete orphaned layouts. */
        foreach(const QnLayoutResourcePtr &layout, context()->resourcePool()->getResourcesWithParentId(QnId()).filtered<QnLayoutResource>())
            if(snapshotManager()->isLocal(layout) && !snapshotManager()->isFile(layout))
                resourcePool()->removeResource(layout);

        /* Close all other layouts. */
        foreach(QnWorkbenchLayout *layout, workbench()->layouts()) {
            QnLayoutResourcePtr resource = layout->resource();
            if(resource->getParentId() != user->getId())
                workbench()->removeLayout(layout);
        }
    }

    submitDelayedDrops();
}

void QnWorkbenchActionHandler::at_workbench_layoutsChanged() {
    if(!workbench()->layouts().empty())
        return;

    menu()->trigger(Qn::OpenNewTabAction);
    //submitDelayedDrops();
}

void QnWorkbenchActionHandler::at_workbench_cellAspectRatioChanged() {
    qreal value = workbench()->currentLayout()->hasCellAspectRatio()
                  ? workbench()->currentLayout()->cellAspectRatio()
                  : qnGlobals->defaultLayoutCellAspectRatio();

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

void QnWorkbenchActionHandler::at_workbench_currentLayoutChanged() {
    action(Qn::RadassAutoAction)->setChecked(true);
    qnRedAssController->setMode(Qn::AutoResolution);
}

void QnWorkbenchActionHandler::at_messageProcessor_connectionClosed() {
    action(Qn::ConnectToServerAction)->setIcon(qnSkin->icon("titlebar/disconnected.png"));
    action(Qn::ConnectToServerAction)->setText(tr("Connect to Server..."));

    if (!mainWindow())
        return;

    if (cameraAdditionDialog())
        cameraAdditionDialog()->hide();
    context()->instance<QnAppServerNotificationCache>()->clear();
}

void QnWorkbenchActionHandler::at_messageProcessor_connectionOpened() {
    action(Qn::ConnectToServerAction)->setIcon(qnSkin->icon("titlebar/connected.png"));
    action(Qn::ConnectToServerAction)->setText(tr("Connect to Another Server...")); // TODO: #GDM use conditional texts?

    context()->instance<QnAppServerNotificationCache>()->getFileList();
}

void QnWorkbenchActionHandler::at_mainMenuAction_triggered() {
    m_mainMenu = menu()->newMenu(Qn::MainScope, mainWindow());

    action(Qn::MainMenuAction)->setMenu(m_mainMenu.data());
}

void QnWorkbenchActionHandler::at_openCurrentUserLayoutMenuAction_triggered() {
    m_currentUserLayoutsMenu = menu()->newMenu(Qn::OpenCurrentUserLayoutMenu, Qn::TitleBarScope);

    action(Qn::OpenCurrentUserLayoutMenu)->setMenu(m_currentUserLayoutsMenu.data());
}

void QnWorkbenchActionHandler::at_layoutCountWatcher_layoutCountChanged() {
    action(Qn::OpenCurrentUserLayoutMenu)->setEnabled(context()->instance<QnWorkbenchUserLayoutCountWatcher>()->layoutCount() > 0);
}

void QnWorkbenchActionHandler::at_nextLayoutAction_triggered() {
    workbench()->setCurrentLayoutIndex((workbench()->currentLayoutIndex() + 1) % workbench()->layouts().size());
}

void QnWorkbenchActionHandler::at_previousLayoutAction_triggered() {
    workbench()->setCurrentLayoutIndex((workbench()->currentLayoutIndex() - 1 + workbench()->layouts().size()) % workbench()->layouts().size());
}

void QnWorkbenchActionHandler::at_openInLayoutAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnLayoutResourcePtr layout = parameters.argument<QnLayoutResourcePtr>(Qn::LayoutResourceRole);
    if(!layout) {
        qnWarning("No layout provided.");
        return;
    }

    QPointF position = parameters.argument<QPointF>(Qn::ItemPositionRole);

    int maxItems = (qnSettings->lightMode() & Qn::LightModeSingleItem)
            ? 1
            : qnSettings->maxSceneVideoItems();

    bool adjustAspectRatio = layout->getItems().isEmpty() || !layout->hasCellAspectRatio();

    QnResourceWidgetList widgets = parameters.widgets();
    if(!widgets.empty() && position.isNull() && layout->getItems().empty()) {
        QHash<QUuid, QnLayoutItemData> itemDataByUuid;
        foreach(const QnResourceWidget *widget, widgets) {
            QnLayoutItemData data = widget->item()->data();
            itemDataByUuid[data.uuid] = data;
        }

        /* Generate new UUIDs. */
        for(QHash<QUuid, QnLayoutItemData>::iterator pos = itemDataByUuid.begin(); pos != itemDataByUuid.end(); pos++)
            pos->uuid = QUuid::createUuid();

        /* Update cross-references. */
        for(QHash<QUuid, QnLayoutItemData>::iterator pos = itemDataByUuid.begin(); pos != itemDataByUuid.end(); pos++)
            if(!pos->zoomTargetUuid.isNull())
                pos->zoomTargetUuid = itemDataByUuid[pos->zoomTargetUuid].uuid;

        /* Add to layout. */
        foreach(const QnLayoutItemData &data, itemDataByUuid) {
            if (layout->getItems().size() >= maxItems)
                return;

            layout->addItem(data);
        }
    } else {
        // TODO: #Elric server & media resources only!

        QnResourceList resources = parameters.resources();
        if(!resources.isEmpty()) {
            AddToLayoutParams addParams;
            addParams.usePosition = !position.isNull();
            addParams.position = position;
            addParams.time = parameters.argument<qint64>(Qn::ItemTimeRole, -1);
            addToLayout(layout, resources, addParams);
        }
    }


    QnWorkbenchLayout *workbenchLayout = workbench()->currentLayout();
    if (adjustAspectRatio && workbenchLayout->resource() == layout) {
        const qreal normalAspectRatio = 4.0 / 3.0;
        const qreal wideAspectRatio = 16.0 / 9.0;

        qreal cellAspectRatio = -1.0;
        qreal midAspectRatio = 0.0;
        int count = 0;


        if (!widgets.isEmpty()) {
            /* Here we don't take into account already added widgets. It's ok because
               we can get here only if the layout doesn't have cell aspect ratio, that means
               its widgets don't have aspect ratio too. */
            foreach (QnResourceWidget *widget, widgets) {
                if (widget->hasAspectRatio()) {
                    midAspectRatio += widget->aspectRatio();
                    ++count;
                }
            }
        } else {
            foreach (QnWorkbenchItem *item, workbenchLayout->items()) {
                QnResourceWidget *widget = context()->display()->widget(item);
                if (widget && widget->hasAspectRatio()) {
                    midAspectRatio += widget->aspectRatio();
                    ++count;
                }
            }
        }

        if (count > 0) {
            midAspectRatio /= count;
            cellAspectRatio = (qAbs(midAspectRatio - normalAspectRatio) < qAbs(midAspectRatio - wideAspectRatio))
                              ? normalAspectRatio : wideAspectRatio;
        }

        if (cellAspectRatio > 0)
            layout->setCellAspectRatio(cellAspectRatio);
        else if (workbenchLayout->items().size() > 1)
            layout->setCellAspectRatio(qnGlobals->defaultLayoutCellAspectRatio());
    }
}

void QnWorkbenchActionHandler::at_openInCurrentLayoutAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    parameters.setArgument(Qn::LayoutResourceRole, workbench()->currentLayout()->resource());
    menu()->trigger(Qn::OpenInLayoutAction, parameters);
}

void QnWorkbenchActionHandler::at_openInNewLayoutAction_triggered() {
    menu()->trigger(Qn::OpenNewTabAction);
    menu()->trigger(Qn::OpenInCurrentLayoutAction, menu()->currentParameters(sender()));
}

void QnWorkbenchActionHandler::at_openInNewWindowAction_triggered() {
    // TODO: #Elric server & media resources only!
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

void QnWorkbenchActionHandler::at_openLayoutsInNewWindowAction_triggered() {
    // TODO: #GDM this won't work for layouts that are not saved. (de)serialization of layouts is not implemented.
    QnLayoutResourceList layouts = menu()->currentParameters(sender()).resources().filtered<QnLayoutResource>();
    if(layouts.isEmpty())
        return;
    openResourcesInNewWindow(layouts);
}

void QnWorkbenchActionHandler::at_openCurrentLayoutInNewWindowAction_triggered() {
    menu()->trigger(Qn::OpenLayoutsInNewWindowAction, workbench()->currentLayout()->resource());
}

void QnWorkbenchActionHandler::at_openNewTabAction_triggered() {
    QnWorkbenchLayout *layout = new QnWorkbenchLayout(this);

    layout->setName(generateUniqueLayoutName(context()->user(), tr("New layout"), tr("New layout %1")));

    workbench()->addLayout(layout);
    workbench()->setCurrentLayout(layout);
}

void QnWorkbenchActionHandler::at_openNewWindowAction_triggered() {
    openNewWindow(QStringList());
}

void QnWorkbenchActionHandler::at_moveCameraAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnResourceList resources = parameters.resources();
    QnMediaServerResourcePtr server = parameters.argument<QnMediaServerResourcePtr>(Qn::MediaServerResourceRole);
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
            mainWindow(),
            errorResources,
            Qn::MainWindow_Tree_DragCameras_Help,
            tr("Error"),
            tr("Camera(s) cannot be moved to server '%1'. It might have been offline since the server is up.").arg(server->getName()),
            QDialogButtonBox::Ok
        );
    }

    if(!modifiedResources.empty()) {
        detail::QnResourceStatusReplyProcessor *processor = new detail::QnResourceStatusReplyProcessor(this, modifiedResources, oldDisabledFlags);
        connection()->saveAsync(modifiedResources, processor, SLOT(at_replyReceived(int, const QnResourceList &, int)));
    }
}

void QnWorkbenchActionHandler::at_dropResourcesAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnResourceList resources = parameters.resources();
    QnLayoutResourceList layouts = resources.filtered<QnLayoutResource>();
    foreach(QnLayoutResourcePtr r, layouts)
        resources.removeOne(r);

    if (workbench()->currentLayout()->resource()->locked() &&
            !resources.empty() &&
            layouts.empty()) {
        QnGraphicsMessageBox::information(tr("Layout is locked and cannot be changed."));
        return;
    }

    if (!resources.empty()) {
        parameters.setResources(resources);
        if (menu()->canTrigger(Qn::OpenInCurrentLayoutAction, parameters))
            menu()->trigger(Qn::OpenInCurrentLayoutAction, parameters);
        else {
            QnLayoutResourcePtr layout = workbench()->currentLayout()->resource();
            if (layout->hasFlags(QnResource::url | QnResource::local | QnResource::layout)) {
                bool hasLocal = false;
                foreach (const QnResourcePtr &resource, resources) {
                    //TODO: #GDM refactor duplicated code
                    hasLocal |= resource->hasFlags(QnResource::url | QnResource::local | QnResource::media)
                            && !resource->getUrl().startsWith(QnLayoutFileStorageResource::layoutPrefix());
                    if (hasLocal)
                        break;
                }
                if (hasLocal)
                    QMessageBox::warning(mainWindow(),
                                         tr("Cannot add item"),
                                         tr("Cannot add a local file to Multi-Video"));
            }
        }
    }
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
    QByteArray data = menu()->currentParameters(sender()).argument<QByteArray>(Qn::SerializedDataRole);
    QDataStream stream(&data, QIODevice::ReadOnly);
    QnMimeData mimeData;
    stream >> mimeData;
    if(stream.status() != QDataStream::Ok || mimeData.formats().empty())
        return;

    m_delayedDrops.push_back(mimeData);

    submitDelayedDrops();
}

void QnWorkbenchActionHandler::at_instantDropResourcesAction_triggered()
{
    QByteArray data = menu()->currentParameters(sender()).argument<QByteArray>(Qn::SerializedDataRole);
    QDataStream stream(&data, QIODevice::ReadOnly);
    QnMimeData mimeData;
    stream >> mimeData;
    if(stream.status() != QDataStream::Ok || mimeData.formats().empty())
        return;
    m_instantDrops.push_back(mimeData);

    submitInstantDrop();
}

void QnWorkbenchActionHandler::at_openFileAction_triggered() {
    QStringList filters;
    //filters << tr("All Supported (*.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp *.jpg *.png *.gif *.bmp *.tiff *.layout)");
    filters << tr("All Supported (*.nov *.avi *.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp *.jpg *.png *.gif *.bmp *.tiff)");
    filters << tr("Video (*.avi *.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp)");
    filters << tr("Pictures (*.jpg *.png *.gif *.bmp *.tiff)");
    //filters << tr("Layouts (*.layout)"); // TODO
    filters << tr("All files (*.*)");

    QStringList files = QFileDialog::getOpenFileNames(mainWindow(),
                                                      tr("Open file"),
                                                      QString(),
                                                      filters.join(lit(";;")),
                                                      0,
                                                      QnCustomFileDialog::fileDialogOptions());

    if (!files.isEmpty())
        menu()->trigger(Qn::DropResourcesAction, addToResourcePool(files));
}

void QnWorkbenchActionHandler::at_openLayoutAction_triggered() {
    QStringList filters;
    filters << tr("All Supported (*.layout)");
    filters << tr("Layouts (*.layout)");
    filters << tr("All files (*.*)");

    QString fileName = QFileDialog::getOpenFileName(mainWindow(),
                                                    tr("Open file"),
                                                    QString(),
                                                    filters.join(lit(";;")),
                                                    0,
                                                    QnCustomFileDialog::fileDialogOptions());

    if(!fileName.isEmpty())
        menu()->trigger(Qn::DropResourcesAction, addToResourcePool(fileName).filtered<QnLayoutResource>());
}

void QnWorkbenchActionHandler::at_openFolderAction_triggered() {
    QString dirName = QFileDialog::getExistingDirectory(mainWindow(),
                                                        tr("Select folder..."),
                                                        QString(),
                                                        QnCustomFileDialog::directoryDialogOptions());

    if(!dirName.isEmpty())
        menu()->trigger(Qn::DropResourcesAction, addToResourcePool(dirName));
}

void QnWorkbenchActionHandler::notifyAboutUpdate(bool alwaysNotify) {
    QnUpdateInfoItem update = context()->instance<QnWorkbenchUpdateWatcher>()->availableUpdate();
    if(update.isNull()) {
        if(alwaysNotify)
            QMessageBox::information(mainWindow(), tr("Information"), tr("No updates available."));
        return;
    }

    QnSoftwareVersion ignoredUpdateVersion = qnSettings->ignoredUpdateVersion();
    bool ignoreThisVersion = update.engineVersion <= ignoredUpdateVersion;
    bool thisVersionWasIgnored = ignoreThisVersion;
    if(ignoreThisVersion && !alwaysNotify)
        return;

    QnCheckableMessageBox::question(
        mainWindow(),
        Qn::Upgrade_Help,
        tr("Software update is available"),
        tr("Version %1 is available for download at <a href=\"%2\">%2</a>.").arg(update.productVersion.toString()).arg(update.url.toString()),
        tr("Don't notify again about this update."),
        &ignoreThisVersion,
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        QDialogButtonBox::Ok
    );

    if(ignoreThisVersion != thisVersionWasIgnored)
        qnSettings->setIgnoredUpdateVersion(ignoreThisVersion ? update.engineVersion : QnSoftwareVersion());
}

void QnWorkbenchActionHandler::openLayoutSettingsDialog(const QnLayoutResourcePtr &layout) {
    if(!layout)
        return;

    if(!accessController()->hasPermissions(layout, Qn::EditLayoutSettingsPermission))
        return;

    QScopedPointer<QnLayoutSettingsDialog> dialog(new QnLayoutSettingsDialog(mainWindow()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setWindowTitle(tr("Layout Settings"));
    dialog->readFromResource(layout);

    bool backgroundWasEmpty = layout->backgroundImageFilename().isEmpty();
    if(!dialog->exec() || !dialog->submitToResource(layout))
        return;

    /* Move layout items to grid center to best fit the background */
    if (backgroundWasEmpty && !layout->backgroundImageFilename().isEmpty()) {
        QnWorkbenchLayout* wlayout = QnWorkbenchLayout::instance(layout);
        if (wlayout)
            wlayout->centralizeItems();
    }
}

void QnWorkbenchActionHandler::at_updateWatcher_availableUpdateChanged() {
    if (qnSettings->isAutoCheckForUpdates() && qnSettings->isUpdatesEnabled())
        notifyAboutUpdate(false);
}

void QnWorkbenchActionHandler::at_checkForUpdatesAction_triggered() {
    notifyAboutUpdate(true);
}

void QnWorkbenchActionHandler::at_showcaseAction_triggered() {
    QDesktopServices::openUrl(qnSettings->showcaseUrl());
}

void QnWorkbenchActionHandler::at_aboutAction_triggered() {
    QScopedPointer<QnAboutDialog> dialog(new QnAboutDialog(mainWindow()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_preferencesGeneralTabAction_triggered() {
    QScopedPointer<QnPreferencesDialog> dialog(new QnPreferencesDialog(context(), mainWindow()));
    dialog->setCurrentPage(QnPreferencesDialog::GeneralPage);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_preferencesLicensesTabAction_triggered() {
    QScopedPointer<QnPreferencesDialog> dialog(new QnPreferencesDialog(context(), mainWindow()));
    dialog->setCurrentPage(QnPreferencesDialog::LicensesPage);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_preferencesServerTabAction_triggered() {
    QScopedPointer<QnPreferencesDialog> dialog(new QnPreferencesDialog(context(), mainWindow()));
    dialog->setCurrentPage(QnPreferencesDialog::ServerPage);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_preferencesNotificationTabAction_triggered() {
    QScopedPointer<QnPreferencesDialog> dialog(new QnPreferencesDialog(context(), mainWindow()));
    dialog->setCurrentPage(QnPreferencesDialog::NotificationsPage);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_businessEventsAction_triggered() {
    menu()->trigger(Qn::OpenBusinessRulesAction);
}

void QnWorkbenchActionHandler::at_openBusinessRulesAction_triggered() {
    bool newlyCreated = false;
    if(!businessRulesDialog()) {
        m_businessRulesDialog = new QnBusinessRulesDialog(mainWindow());
        newlyCreated = true;
    }

    QString filter;
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnVirtualCameraResourceList cameras = parameters.resources().filtered<QnVirtualCameraResource>();
    if (!cameras.isEmpty()) {
        foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
            filter += camera->getPhysicalId(); //getUniqueId() cannot be used here --gdm
        }
    }
    businessRulesDialog()->setFilter(filter);

    QRect oldGeometry = businessRulesDialog()->geometry();
    businessRulesDialog()->show();
    businessRulesDialog()->raise();
    businessRulesDialog()->activateWindow(); // TODO: #Elric show raise activateWindow? Maybe we should also do grabKeyboard, grabMouse? wtf, really?
    if(!newlyCreated)
        businessRulesDialog()->setGeometry(oldGeometry);
}

void QnWorkbenchActionHandler::at_webClientAction_triggered() {
    QUrl url(QnAppServerConnectionFactory::defaultUrl());
    url.setUserName(QString());
    url.setPassword(QString());
    url.setPath(QLatin1String("/web/"));
    QDesktopServices::openUrl(url);
}

void QnWorkbenchActionHandler::at_businessEventsLogAction_triggered() {
    menu()->trigger(Qn::OpenBusinessLogAction);
}

void QnWorkbenchActionHandler::at_openBusinessLogAction_triggered() {
    bool newlyCreated = false;
    if(!businessEventsLogDialog()) {
        m_businessEventsLogDialog = new QnEventLogDialog(mainWindow(), context());
        newlyCreated = true;
    }

    QnActionParameters parameters = menu()->currentParameters(sender());

    BusinessEventType::Value eventType = parameters.argument(Qn::EventTypeRole, BusinessEventType::AnyBusinessEvent);
    QnVirtualCameraResourceList cameras = parameters.resources().filtered<QnVirtualCameraResource>();

    // show diagnostics if Issues action was triggered
    if (eventType != BusinessEventType::AnyBusinessEvent || !cameras.isEmpty()) {
        businessEventsLogDialog()->disableUpdateData();
        businessEventsLogDialog()->setEventType(eventType);
        businessEventsLogDialog()->setActionType(BusinessActionType::Diagnostics);
        QDate date = QDateTime::currentDateTime().date();
        businessEventsLogDialog()->setDateRange(date, date);
        businessEventsLogDialog()->setCameraList(cameras);
        businessEventsLogDialog()->enableUpdateData();
    }

    QRect oldGeometry = businessEventsLogDialog()->geometry();
    businessEventsLogDialog()->show();
    businessEventsLogDialog()->raise();
    businessEventsLogDialog()->activateWindow(); // TODO: #Elric show raise activateWindow? Maybe we should also do grabKeyboard, grabMouse? wtf, really?
    if(!newlyCreated)
        businessEventsLogDialog()->setGeometry(oldGeometry);
}

void QnWorkbenchActionHandler::at_cameraListAction_triggered()
{
    QnMediaServerResourcePtr server = menu()->currentParameters(sender()).resource().dynamicCast<QnMediaServerResource>();

    bool newlyCreated = false;
    if(!cameraListDialog()) {
        m_cameraListDialog = new QnCameraListDialog(mainWindow());
        newlyCreated = true;
    }
    QRect oldGeometry = cameraListDialog()->geometry();
    cameraListDialog()->setServer(server);
    cameraListDialog()->show();
    cameraListDialog()->raise();
    cameraListDialog()->activateWindow(); // TODO: #Elric show raise activateWindow? Maybe we should also do grabKeyboard, grabMouse? wtf, really?
    if(!newlyCreated)
        cameraListDialog()->setGeometry(oldGeometry);
}

void QnWorkbenchActionHandler::at_connectToServerAction_triggered() {
    const QUrl lastUsedUrl = qnSettings->lastUsedConnection().url;
    if (lastUsedUrl.isValid() && lastUsedUrl != QnAppServerConnectionFactory::defaultUrl())
        return;

    if (!loginDialog()) {
        m_loginDialog = new QnLoginDialog(mainWindow(), context());
        loginDialog()->setModal(true);
    }
    while(true) {
        QnActionParameters parameters = menu()->currentParameters(sender());
        loginDialog()->setAutoConnect(parameters.argument(Qn::AutoConnectRole, false));

        if(!loginDialog()->exec())
            return;

        if(!context()->user())
            break; /* We weren't connected, so layouts cannot be saved. */

        QnWorkbenchState state;
        workbench()->submit(state);

        if(!context()->instance<QnWorkbenchLayoutsHandler>()->closeAllLayouts(true))
            continue;

        QnWorkbenchStateHash states = qnSettings->userWorkbenchStates();
        states[context()->user()->getName()] = state;
        qnSettings->setUserWorkbenchStates(states);
        break;
    }
    menu()->trigger(Qn::ClearCameraSettingsAction);

    QnConnectionDataList connections = qnSettings->customConnections();

    QnConnectionData connectionData;
    connectionData.url = loginDialog()->currentUrl();
    qnSettings->setLastUsedConnection(connectionData);

    qnSettings->setStoredPassword(loginDialog()->rememberPassword()
                ? connectionData.url.password()
                : QString()
                  );

    // remove previous "Last used connection"
    connections.removeOne(QnConnectionDataList::defaultLastUsedNameKey());

    QUrl cleanUrl(connectionData.url);
    cleanUrl.setPassword(QString());
    QnConnectionData selected = connections.getByName(loginDialog()->currentName());
    if (selected.url == cleanUrl){
        connections.removeOne(selected.name);
        connections.prepend(selected);
    } else {
        // save "Last used connection"
        QnConnectionData last(connectionData);
        last.name = QnConnectionDataList::defaultLastUsedNameKey();
        last.url.setPassword(QString());
        connections.prepend(last);
    }
    qnSettings->setCustomConnections(connections);

    //updateStoredConnections(connectionData);

    if (loginDialog()->restartPending())
        QTimer::singleShot(10, this, SLOT(at_exitAction_triggered()));
    else
        menu()->trigger(Qn::ReconnectAction, QnActionParameters().withArgument(Qn::ConnectionInfoRole, loginDialog()->currentInfo()));
}

void QnWorkbenchActionHandler::at_reconnectAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    const QnConnectionData connectionData = qnSettings->lastUsedConnection();
    if (!connectionData.isValid())
        return;

    QnConnectionInfoPtr connectionInfo = parameters.argument<QnConnectionInfoPtr>(Qn::ConnectionInfoRole);
    if(connectionInfo.isNull()) {
        QnAppServerConnectionPtr connection = QnAppServerConnectionFactory::createConnection(connectionData.url);

        QnConnectionRequestResult result;
        connection->connectAsync(&result, SLOT(processReply(int, const QVariant &, int)));
        if(result.exec() != 0)
            return;

        connectionInfo = result.reply<QnConnectionInfoPtr>();
    }

    // TODO: #Elric maybe we need to check server-client compatibility here? --done //GDM
    { // I think we should move this common code to common place --gdm
        bool compatibleProduct = qnSettings->isDevMode() || connectionInfo->brand.isEmpty()
                || connectionInfo->brand == QLatin1String(QN_PRODUCT_NAME_SHORT);
        if (!compatibleProduct)
            return;

        QnCompatibilityChecker remoteChecker(connectionInfo->compatibilityItems);
        QnCompatibilityChecker localChecker(localCompatibilityItems());
        QnCompatibilityChecker* compatibilityChecker;
        if (remoteChecker.size() > localChecker.size()) {
            compatibilityChecker = &remoteChecker;
        } else {
            compatibilityChecker = &localChecker;
        }

        if (!compatibilityChecker->isCompatible(QLatin1String("Client"), QnSoftwareVersion(QN_ENGINE_VERSION), QLatin1String("ECS"), connectionInfo->version))
            return;
    }



    QnAppServerConnectionFactory::setDefaultMediaProxyPort(connectionInfo->proxyPort);

    QnClientMessageProcessor::instance()->stop(); // TODO: #Elric blocks gui thread.
    QnSessionManager::instance()->stop();

    QnAppServerConnectionFactory::setCurrentVersion(connectionInfo->version);
    QnAppServerConnectionFactory::setDefaultUrl(connectionData.url);

    // repopulate the resource pool
    QnResource::stopCommandProc();
    QnResourceDiscoveryManager::instance()->stop();

#ifndef STANDALONE_MODE
    static const char *appserverAddedPropertyName = "_qn_appserverAdded";
    if(!QnResourceDiscoveryManager::instance()->property(appserverAddedPropertyName).toBool()) {
        QnResourceDiscoveryManager::instance()->setProperty(appserverAddedPropertyName, true);
    }
#endif

    // don't remove local resources
    const QnResourceList remoteResources = resourcePool()->getResourcesWithFlag(QnResource::remote);
    resourcePool()->setLayoutsUpdated(false);
    resourcePool()->removeResources(remoteResources);
    // Also remove layouts that were just added and have no 'remote' flag set.
    //TODO: #Elric hack.
    foreach(const QnLayoutResourcePtr &layout, resourcePool()->getResources().filtered<QnLayoutResource>())
        if(!(snapshotManager()->flags(layout) & Qn::ResourceIsLocal))
            resourcePool()->removeResource(layout);
    resourcePool()->setLayoutsUpdated(true);

    qnLicensePool->reset();

    notificationsHandler()->clear();

    QnSessionManager::instance()->start();
    QnClientMessageProcessor::instance()->run();

    QnResourceDiscoveryManager::instance()->start();

    QnResource::startCommandProc();

    context()->setUserName(connectionData.url.userName());

    at_messageProcessor_connectionOpened();
}

void QnWorkbenchActionHandler::at_disconnectAction_triggered() {
    if(context()->user() && !context()->instance<QnWorkbenchLayoutsHandler>()->closeAllLayouts(true))
        return;

    // TODO: #GDM Factor out common code from reconnect/disconnect/login actions.

    menu()->trigger(Qn::ClearCameraSettingsAction);

    QnClientMessageProcessor::instance()->stop(); // TODO: #Elric blocks gui thread.
//    QnSessionManager::instance()->stop(); // omfg... logic sucks
    QnResource::stopCommandProc();
    QnResourceDiscoveryManager::instance()->stop();

    // don't remove local resources
    const QnResourceList remoteResources = resourcePool()->getResourcesWithFlag(QnResource::remote);
    resourcePool()->setLayoutsUpdated(false);
    resourcePool()->removeResources(remoteResources);
    // Also remove layouts that were just added and have no 'remote' flag set.
    //TODO: #Elric hack.
    foreach(const QnLayoutResourcePtr &layout, resourcePool()->getResources().filtered<QnLayoutResource>())
        if(!(snapshotManager()->flags(layout) & Qn::ResourceIsLocal))
            resourcePool()->removeResource(layout);
    resourcePool()->setLayoutsUpdated(true);

    qnLicensePool->reset();

    QnAppServerConnectionFactory::setCurrentVersion(QnSoftwareVersion());
    // TODO: #Elric save workbench state on logout.

    notificationsHandler()->clear();

    qnSettings->setStoredPassword(QString());
}

void QnWorkbenchActionHandler::at_thumbnailsSearchAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnResourcePtr resource = parameters.resource();
    if(!resource)
        return;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    if(period.isEmpty())
        return;

    QnTimePeriodList periods = parameters.argument<QnTimePeriodList>(Qn::TimePeriodsRole);

    /* List of possible time steps, in milliseconds. */
    const qint64 steps[] = {
        1000ll * 10,                    /* 10 seconds. */
        1000ll * 15,                    /* 15 seconds. */
        1000ll * 20,                    /* 20 seconds. */
        1000ll * 30,                    /* 30 seconds. */
        1000ll * 60,                    /* 1 minute. */
        1000ll * 60 * 2,                /* 2 minutes. */
        1000ll * 60 * 3,                /* 3 minutes. */
        1000ll * 60 * 5,                /* 5 minutes. */
        1000ll * 60 * 10,               /* 10 minutes. */
        1000ll * 60 * 15,               /* 15 minutes. */
        1000ll * 60 * 20,               /* 20 minutes. */
        1000ll * 60 * 30,               /* 30 minutes. */
        1000ll * 60 * 60,               /* 1 hour. */
        1000ll * 60 * 60 * 2,           /* 2 hours. */
        1000ll * 60 * 60 * 3,           /* 3 hours. */
        1000ll * 60 * 60 * 6,           /* 6 hours. */
        1000ll * 60 * 60 * 12,          /* 12 hours. */
        1000ll * 60 * 60 * 24,          /* 1 day. */
        1000ll * 60 * 60 * 24 * 2,      /* 2 days. */
        1000ll * 60 * 60 * 24 * 3,      /* 3 days. */
        1000ll * 60 * 60 * 24 * 5,      /* 5 days. */
        1000ll * 60 * 60 * 24 * 10,     /* 10 days. */
        1000ll * 60 * 60 * 24 * 20,     /* 20 days. */
        1000ll * 60 * 60 * 24 * 30,     /* 30 days. */
        0,
    };

    const qint64 maxItems = qnSettings->maxPreviewSearchItems();

    if(period.durationMs < steps[1]) {
        QMessageBox::warning(mainWindow(), tr("Could not perform preview search"), tr("Selected time period is too short to perform preview search. Please select a longer period."), QMessageBox::Ok);
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
                int startMSecs = qFloor(QDateTime(startDateTime.date()).msecsTo(startDateTime), step);
                int endMSecs = qCeil(QDateTime(endDateTime.date()).msecsTo(endDateTime), step);

                startDateTime.setTime(QTime(0, 0, 0, 0));
                startDateTime = startDateTime.addMSecs(startMSecs);

                endDateTime.setTime(QTime(0, 0, 0, 0));
                endDateTime = endDateTime.addMSecs(endMSecs);
            } else {
                int stepDays = step / dayMSecs;

                startDateTime.setTime(QTime(0, 0, 0, 0));
                startDateTime.setDate(QDate::fromJulianDay(qFloor(startDateTime.date().toJulianDay(), stepDays)));

                endDateTime.setTime(QTime(0, 0, 0, 0));
                endDateTime.setDate(QDate::fromJulianDay(qCeil(endDateTime.date().toJulianDay(), stepDays)));
            }

            period = QnTimePeriod(startDateTime.toMSecsSinceEpoch(), endDateTime.toMSecsSinceEpoch() - startDateTime.toMSecsSinceEpoch());
        } else {
            qint64 startTime = qFloor(period.startTimeMs, step);
            qint64 endTime = qCeil(period.endTimeMs(), step);
            period = QnTimePeriod(startTime, endTime - startTime);
        }

        itemCount = qMin(period.durationMs / step, maxItems);
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
    qreal desiredAspectRatio = qnGlobals->defaultLayoutCellAspectRatio();
    QnResourceWidget *widget = parameters.widget();
    if (widget && widget->hasAspectRatio())
        desiredAspectRatio = widget->aspectRatio();

    const int matrixWidth = qMax(1, qRound(std::sqrt(desiredAspectRatio * itemCount)));

    /* Construct and add a new layout. */
    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setGuid(QUuid::createUuid().toString());
    layout->setName(tr("Preview Search for %1").arg(resource->getName()));
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
        item.contrastParams = widget->item()->imageEnhancement();
        item.dewarpingParams = widget->item()->dewarpingParams();
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
    layout->setData(Qn::LayoutCellAspectRatioRole, desiredAspectRatio);
    layout->setLocalRange(period);

    resourcePool()->addResource(layout);
    menu()->trigger(Qn::OpenSingleLayoutAction, layout);
}

void QnWorkbenchActionHandler::at_cameraSettingsAction_triggered() {
    QnVirtualCameraResourceList resources = menu()->currentParameters(sender()).resources().filtered<QnVirtualCameraResource>();
    bool newlyCreated = false;
    if(!cameraSettingsDialog()) {
        m_cameraSettingsDialog = new QnCameraSettingsDialog(mainWindow());
        newlyCreated = true;

        connect(cameraSettingsDialog(), SIGNAL(buttonClicked(QDialogButtonBox::StandardButton)),        this, SLOT(at_cameraSettingsDialog_buttonClicked(QDialogButtonBox::StandardButton)));
        connect(cameraSettingsDialog(), SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)),  this, SLOT(at_cameraSettingsDialog_scheduleExported(const QnVirtualCameraResourceList &)));
        connect(cameraSettingsDialog(), SIGNAL(rejected()),                                             this, SLOT(at_cameraSettingsDialog_rejected()));
        connect(cameraSettingsDialog(), SIGNAL(advancedSettingChanged()),                               this, SLOT(at_cameraSettingsDialog_advancedSettingChanged()));
        connect(cameraSettingsDialog(), SIGNAL(cameraOpenRequested()),                                  this, SLOT(at_cameraSettingsDialog_cameraOpenRequested()));
    }

    if(cameraSettingsDialog()->widget()->resources() != resources) {
        if(cameraSettingsDialog()->isVisible() && (
           cameraSettingsDialog()->widget()->hasDbChanges() || cameraSettingsDialog()->widget()->hasCameraChanges()))
        {
            QDialogButtonBox::StandardButton button = QnResourceListDialog::exec(
                mainWindow(),
                QnResourceList(cameraSettingsDialog()->widget()->resources()),
                tr("Camera(s) not Saved"),
                tr("Save changes to the following %n camera(s)?", "", cameraSettingsDialog()->widget()->resources().size()),
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

void QnWorkbenchActionHandler::at_pictureSettingsAction_triggered() {
    QnResourcePtr resource = menu()->currentParameters(sender()).resource();
    if (!resource)
        return;

    QnMediaResourcePtr media = resource.dynamicCast<QnMediaResource>();
    if (!media)
        return;

    QScopedPointer<QnPictureSettingsDialog> dialog(new QnPictureSettingsDialog(mainWindow()));
    dialog->updateFromResource(media);
    if (dialog->exec()) {
        dialog->submitToResource(media);
    } else {
        QnResourceWidget* centralWidget = display()->widget(Qn::CentralRole);
        if (QnMediaResourceWidget* mediaWidget = dynamic_cast<QnMediaResourceWidget*>(centralWidget))
            mediaWidget->setDewarpingParams(media->getDewarpingParams());
    }
}

void QnWorkbenchActionHandler::at_cameraIssuesAction_triggered()
{
    menu()->trigger(Qn::OpenBusinessLogAction,
                    menu()->currentParameters(sender())
                    .withArgument(Qn::EventTypeRole, BusinessEventType::AnyCameraIssue));
}

void QnWorkbenchActionHandler::at_cameraBusinessRulesAction_triggered() {
    menu()->trigger(Qn::OpenBusinessRulesAction,
                    menu()->currentParameters(sender()));
}

void QnWorkbenchActionHandler::at_cameraDiagnosticsAction_triggered() {
    QnVirtualCameraResourcePtr resource = menu()->currentParameters(sender()).resource().dynamicCast<QnVirtualCameraResource>();
    if(!resource)
        return;

    QScopedPointer<QnCameraDiagnosticsDialog> dialog(new QnCameraDiagnosticsDialog(mainWindow()));
    dialog->setResource(resource);
    dialog->restart();
    dialog->exec();
}

void QnWorkbenchActionHandler::at_clearCameraSettingsAction_triggered() {
    if(cameraSettingsDialog() && cameraSettingsDialog()->isVisible())
        menu()->trigger(Qn::OpenInCameraSettingsDialogAction, QnResourceList());
}

void QnWorkbenchActionHandler::at_cameraSettingsDialog_buttonClicked(QDialogButtonBox::StandardButton button) {
    switch(button) {
    case QDialogButtonBox::Ok:
    case QDialogButtonBox::Apply:
        saveCameraSettingsFromDialog(true);
        break;
    case QDialogButtonBox::Cancel:
        cameraSettingsDialog()->widget()->reject();
        break;
    default:
        break;
    }
}

void QnWorkbenchActionHandler::at_cameraSettingsDialog_cameraOpenRequested() {
    QnResourceList resources = cameraSettingsDialog()->widget()->resources();
    menu()->trigger(Qn::OpenInNewLayoutAction, resources);

    cameraSettingsDialog()->widget()->setResources(resources);
    m_selectionUpdatePending = false;
}

void QnWorkbenchActionHandler::at_cameraSettingsDialog_scheduleExported(const QnVirtualCameraResourceList &cameras){
    connection()->saveAsync(cameras, this, SLOT(at_resources_saved(int, const QnResourceList &, int)));
}

void QnWorkbenchActionHandler::at_cameraSettingsDialog_rejected() {
    cameraSettingsDialog()->widget()->updateFromResources();
}

void QnWorkbenchActionHandler::at_cameraSettingsDialog_advancedSettingChanged() {
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

    QnMediaServerResourcePtr server = resources[0];

    bool newlyCreated = false;
    if(!cameraAdditionDialog()) {
        m_cameraAdditionDialog = new QnCameraAdditionDialog(mainWindow());
        newlyCreated = true;
    }
    QnCameraAdditionDialog* dialog = cameraAdditionDialog();

    if (dialog->server() != server) {
        if (dialog->state() == QnCameraAdditionDialog::Searching
                || dialog->state() == QnCameraAdditionDialog::Adding) {

            int result = QMessageBox::warning(
                        mainWindow(),
                        tr("Process is in progress"),
                        tr("Camera addition is already in progress."\
                           "Are you sure you want to cancel current process?"), //TODO: #GDM show current process details
                        QMessageBox::Ok | QMessageBox::Cancel,
                        QMessageBox::Cancel
            );
            if (result != QMessageBox::Ok)
                return;
        }
        dialog->setServer(server);
    }

    QRect oldGeometry = cameraAdditionDialog()->geometry();
    cameraAdditionDialog()->show();
    if(!newlyCreated)
        cameraAdditionDialog()->setGeometry(oldGeometry);
}

void QnWorkbenchActionHandler::at_serverSettingsAction_triggered() {
    QnMediaServerResourcePtr server = menu()->currentParameters(sender()).resource().dynamicCast<QnMediaServerResource>();
    if(!server)
        return;

    QScopedPointer<QnServerSettingsDialog> dialog(new QnServerSettingsDialog(server, mainWindow()));
    dialog->setWindowModality(Qt::ApplicationModal);
    if(!dialog->exec())
        return;

    // TODO: #Elric move submitToResources here.
    connection()->saveAsync(server, this, SLOT(at_resources_saved(int, const QnResourceList &, int)));
}

void QnWorkbenchActionHandler::at_serverLogsAction_triggered() {
    QnMediaServerResourcePtr server = menu()->currentParameters(sender()).resource().dynamicCast<QnMediaServerResource>();
    if(!server)
        return;

    QUrl serverUrl = server->getApiUrl();
    
    // TODO: #Elric total encapsulation failure, there should be no proxy-related logic here.
    QUrl url;
    if(!server->getProxyHost().isEmpty()) {
        url.setScheme(lit("http"));
        url.setHost(server->getProxyHost());
        url.setPort(server->getProxyPort());
        url.setPath(lit("/proxy/%4:%5/api/showLog").arg(serverUrl.host()).arg(serverUrl.port()));
    } else {
        url = serverUrl;
        url.setPath(lit("/api/showLog"));
    }
    url.setQuery(lit("lines=1000"));

    QnConnectionData lastUsedConnection = qnSettings->lastUsedConnection();
    url.setUserName(lastUsedConnection.url.userName());
    url.setPassword(lastUsedConnection.url.password());
    
    QDesktopServices::openUrl(url);
}

void QnWorkbenchActionHandler::at_serverIssuesAction_triggered() {
    menu()->trigger(Qn::OpenBusinessLogAction,
                    QnActionParameters().withArgument(Qn::EventTypeRole, BusinessEventType::AnyServerIssue));
}

void QnWorkbenchActionHandler::at_pingAction_triggered() {
    QnResourcePtr resource = menu()->currentParameters(sender()).resource();
    if (!resource)
        return;

#ifdef Q_OS_WIN
    QUrl url = QUrl::fromUserInput(resource->getUrl());
    QString host = url.host();
    QString cmd = QLatin1String("cmd /C ping %1 -t");
    QProcess::startDetached(cmd.arg(host));
#endif
#ifdef Q_OS_LINUX
    QUrl url = QUrl::fromUserInput(resource->getUrl());
    QString host = url.host();
    QString cmd = QLatin1String("xterm -e ping %1");
    QProcess::startDetached(cmd.arg(host));
#endif
#ifdef Q_OS_MACX
    QnConnectionTestingDialog dialog;
    dialog.testResource(resource);
    dialog.exec();
#endif

}

void QnWorkbenchActionHandler::at_youtubeUploadAction_triggered() {
    /* QnResourcePtr resource = menu()->currentParameters(sender()).resource();
    if(resource.isNull())
        return;

    QScopedPointer<YouTubeUploadDialog> dialog(new YouTubeUploadDialog(context(), resource, widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec(); */
}

void QnWorkbenchActionHandler::at_openInFolderAction_triggered() {
    QnResourcePtr resource = menu()->currentParameters(sender()).resource();
    if(resource.isNull())
        return;

    QnEnvironment::showInGraphicalShell(mainWindow(), resource->getUrl());
}

void QnWorkbenchActionHandler::at_deleteFromDiskAction_triggered() {
    QSet<QnResourcePtr> resources = menu()->currentParameters(sender()).resources().toSet();

    QDialogButtonBox::StandardButton button = QnResourceListDialog::exec(
        mainWindow(),
        resources.toList(),
        tr("Delete Files"),
        tr("Are you sure you want to permanently delete these %n file(s)?", "", resources.size()),
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
            mainWindow(),
            QnActionParameterTypes::resources(items),
            Qn::RemoveItems_Help,
            tr("Remove Items"),
            tr("Are you sure you want to remove these %n item(s) from layout?", "", items.size()),
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

    QString name = parameters.argument<QString>(Qn::ResourceNameRole).trimmed();
    if(name.isEmpty()) {
        bool ok = false;
        name = QInputDialog::getText(mainWindow(),
                                     tr("Rename"),
                                     tr("Enter new name for the selected item:"),
                                     QLineEdit::Normal,
                                     resource->getName(),
                                     &ok);
        if (!ok || name.isEmpty())
            return;
    }

    if(name == resource->getName())
        return;

    if(QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>()) {
        context()->instance<QnWorkbenchLayoutsHandler>()->renameLayout(layout, name);
    } else {
        resource->setName(name);
        connection()->saveAsync(resource, this, SLOT(at_resources_saved(int, const QnResourceList &, int)));
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
            mainWindow(),
            resources,
            tr("Delete Resources"),
            tr("Do you really want to delete the following %n item(s)?", "", resources.size()),
            QDialogButtonBox::Yes | QDialogButtonBox::No
        );
        okToDelete = button == QDialogButtonBox::Yes;
    }

    if(!okToDelete)
        return; /* User does not want it deleted. */

    foreach(const QnResourcePtr &resource, resources) {
        if(QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>()) {
            if(snapshotManager()->isLocal(layout)) {
                resourcePool()->removeResource(resource); /* This one can be simply deleted from resource pool. */
                continue;
            }
        }

        connection()->deleteAsync(resource, this, SLOT(at_resource_deleted(const QnHTTPRawResponse&, int)));
    }
}

void QnWorkbenchActionHandler::at_newUserAction_triggered() {
    QnUserResourcePtr user(new QnUserResource());
    user->setPermissions(Qn::GlobalLiveViewerPermissions);

    QScopedPointer<QnUserSettingsDialog> dialog(new QnUserSettingsDialog(context(), mainWindow()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setEditorPermissions(accessController()->globalPermissions());
    dialog->setUser(user);
    dialog->setElementFlags(QnUserSettingsDialog::CurrentPassword, 0);
    setHelpTopic(dialog.data(), Qn::NewUser_Help);

    if(!dialog->exec())
        return;

    dialog->submitToResource();
    user->setGuid(QUuid::createUuid().toString());

    connection()->saveAsync(user, this, SLOT(at_resources_saved(int, const QnResourceList &, int)));
    user->setPassword(QString()); // forget the password now
}

void QnWorkbenchActionHandler::at_exitAction_triggered() {
    if(context()->user()) { // TODO: #Elric factor out
        QnWorkbenchState state;
        workbench()->submit(state);

        QnWorkbenchStateHash states = qnSettings->userWorkbenchStates();
        states[context()->user()->getName()] = state;
        qnSettings->setUserWorkbenchStates(states);
    }

    menu()->trigger(Qn::ClearCameraSettingsAction);
    if(context()->instance<QnWorkbenchLayoutsHandler>()->closeAllLayouts(true)) {
        qApp->exit(0);
        applauncher::scheduleProcessKill( QCoreApplication::applicationPid(), PROCESS_TERMINATE_TIMEOUT );
    }

}

QnAdjustVideoDialog* QnWorkbenchActionHandler::adjustVideoDialog()
{
    if (!m_adjustVideoDialog)
        m_adjustVideoDialog = new QnAdjustVideoDialog(mainWindow());
    return m_adjustVideoDialog.data();
}

void QnWorkbenchActionHandler::at_adjustVideoAction_triggered()
{
    QnMediaResourceWidget *widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if(!widget)
        return;

    adjustVideoDialog()->setWidget(widget);
    adjustVideoDialog()->show();
}

void QnWorkbenchActionHandler::at_userSettingsAction_triggered() {
    QnActionParameters params = menu()->currentParameters(sender());
    QnUserResourcePtr user = params.resource().dynamicCast<QnUserResource>();
    if(!user)
        return;

    Qn::Permissions permissions = accessController()->permissions(user);
    if(!(permissions & Qn::ReadPermission))
        return;

    QScopedPointer<QnUserSettingsDialog> dialog(new QnUserSettingsDialog(context(), mainWindow()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setWindowTitle(tr("User Settings"));
    setHelpTopic(dialog.data(), Qn::UserSettings_Help);

    dialog->setFocusedElement(params.argument<QString>(Qn::FocusElementRole));

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

    QnUserSettingsDialog::ElementFlags emailFlags =
        ((permissions & Qn::ReadEmailPermission) ? QnUserSettingsDialog::Visible : zero) |
        ((permissions & Qn::WriteEmailPermission) ? QnUserSettingsDialog::Editable : zero);
    emailFlags &= flags;

    dialog->setElementFlags(QnUserSettingsDialog::Login, loginFlags);
    dialog->setElementFlags(QnUserSettingsDialog::Password, passwordFlags);
    dialog->setElementFlags(QnUserSettingsDialog::AccessRights, accessRightsFlags);
    dialog->setElementFlags(QnUserSettingsDialog::Email, emailFlags);
    dialog->setEditorPermissions(accessController()->globalPermissions());


    // TODO #elric: This is a totally evil hack. Store password hash/salt in user.
    QString currentPassword = qnSettings->lastUsedConnection().url.password();
    if(user == context()->user()) {
        dialog->setElementFlags(QnUserSettingsDialog::CurrentPassword, passwordFlags);
        dialog->setCurrentPassword(currentPassword);
    } else {
        dialog->setElementFlags(QnUserSettingsDialog::CurrentPassword, 0);
    }

    dialog->setUser(user);
    if(!dialog->exec() || !dialog->hasChanges())
        return;

    if(permissions & Qn::SavePermission) {
        dialog->submitToResource();

        connection()->saveAsync(user, this, SLOT(at_resources_saved(int, const QnResourceList &, int)));

        QString newPassword = user->getPassword();
        user->setPassword(QString());
        if(user == context()->user() && !newPassword.isEmpty() && newPassword != currentPassword) {
            /* Password was changed. Change it in global settings and hope for the best. */
            QnConnectionData data = qnSettings->lastUsedConnection();
            data.url.setPassword(newPassword);
            qnSettings->setLastUsedConnection(data);

            QnAppServerConnectionFactory::setDefaultUrl(data.url);
        }
    }
}

void QnWorkbenchActionHandler::at_layoutSettingsAction_triggered() {
    QnActionParameters params = menu()->currentParameters(sender());
    openLayoutSettingsDialog(params.resource().dynamicCast<QnLayoutResource>());
}

void QnWorkbenchActionHandler::at_currentLayoutSettingsAction_triggered() {
    openLayoutSettingsDialog(workbench()->currentLayout()->resource());
}


void QnWorkbenchActionHandler::at_camera_settings_saved(int httpStatusCode, const QList<QPair<QString, bool> >& operationResult)
{
    QString error = httpStatusCode == 0? lit("Possibly, appropriate camera's service is unavailable now"):
        lit("Mediaserver returned the following error code : ") + httpStatusCode; // #TR #Elric

    QString failedParams;
    QList<QPair<QString, bool> >::ConstIterator it = operationResult.begin();
    for (; it != operationResult.end(); ++it)
    {
        if (!it->second) {
            QString formattedParam(lit("Advanced->") + it->first.right(it->first.length() - 2));
            failedParams += lit("\n");
            failedParams += formattedParam.replace(lit("%%"), lit("->")); // TODO: #Elric #TR
        }
    }

    if (!failedParams.isEmpty()) {
        QMessageBox::warning(
            mainWindow(),
            tr("Could not save parameters"),
            tr("Failed to save the following parameters (%1):\n%2").arg(error, failedParams)
        );

        cameraSettingsDialog()->widget()->updateFromResources();
    }
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

void QnWorkbenchActionHandler::at_createZoomWindowAction_triggered() {
    QnActionParameters params = menu()->currentParameters(sender());

    QnMediaResourceWidget *widget = params.widget<QnMediaResourceWidget>();
    if(!widget)
        return;

    QRectF rect = params.argument<QRectF>(Qn::ItemZoomRectRole, QRectF(0.25, 0.25, 0.5, 0.5));
    AddToLayoutParams addParams;
    addParams.usePosition = true;
    addParams.position = widget->item()->combinedGeometry().center();
    addParams.zoomWindow = rect;
    addParams.dewarpingParams.enabled = widget->dewarpingParams().enabled;
    addParams.zoomUuid = widget->item()->uuid();
    addParams.frameColor = params.argument<QColor>(Qn::ItemFrameColorRole);
    addParams.rotation = widget->item()->rotation();

    addToLayout(workbench()->currentLayout()->resource(), widget->resource()->toResourcePtr(), addParams);
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
    setResolutionMode(Qn::AutoResolution);
}

void QnWorkbenchActionHandler::at_radassLowAction_triggered() {
    setResolutionMode(Qn::LowResolution);
}

void QnWorkbenchActionHandler::at_radassHighAction_triggered() {
    setResolutionMode(Qn::HighResolution);
}

void QnWorkbenchActionHandler::at_setAsBackgroundAction_triggered() {
    if (!context()->user() || !workbench()->currentLayout()->resource())
        return; // action should not be triggered while we are not connected

    if(!accessController()->hasPermissions(workbench()->currentLayout()->resource(), Qn::EditLayoutSettingsPermission))
        return;

    if (workbench()->currentLayout()->resource()->locked())
        return;

    QnProgressDialog *progressDialog = new QnProgressDialog(mainWindow());
    progressDialog->setWindowTitle(tr("Updating background"));
    progressDialog->setLabelText(tr("Image processing can take a lot of time. Please be patient."));
    progressDialog->setRange(0, 0);
    progressDialog->setCancelButton(NULL);
    connect(progressDialog,   SIGNAL(canceled()),                   progressDialog,     SLOT(deleteLater()));

    QnAppServerImageCache *cache = new QnAppServerImageCache(this);
    cache->setProperty(uploadingImageARPropertyName, menu()->currentParameters(sender()).widget()->aspectRatio());
    connect(cache,            SIGNAL(fileUploaded(QString, bool)),  this,               SLOT(at_backgroundImageStored(QString, bool)));
    connect(cache,            SIGNAL(fileUploaded(QString,bool)),   progressDialog,     SLOT(deleteLater()));
    connect(cache,            SIGNAL(fileUploaded(QString, bool)),  cache,              SLOT(deleteLater()));

    cache->storeImage(menu()->currentParameters(sender()).resource()->getUrl());
    progressDialog->exec();
}

void QnWorkbenchActionHandler::at_backgroundImageStored(const QString &filename, bool success) {
    if (!context()->user())
        return; // action should not be triggered while we are not connected

    QnWorkbenchLayout* wlayout = workbench()->currentLayout();
    if (!wlayout)
        return; //security check

    QnLayoutResourcePtr layout = wlayout->resource();
    if (!layout)
        return; //security check

    if(!accessController()->hasPermissions(layout, Qn::EditLayoutSettingsPermission))
        return;

    if (layout->locked())
        return;

    if (!success) {
        QMessageBox::warning(mainWindow(), tr("Error"), tr("Image cannot be uploaded"));
        return;
    }

    layout->setBackgroundImageFilename(filename);
    if (qFuzzyCompare(layout->backgroundOpacity(), 0.0))
        layout->setBackgroundOpacity(0.7);

    wlayout->centralizeItems();
    QRect brect = wlayout->boundingRect();

    int minWidth = qMax(brect.width(), qnGlobals->layoutBackgroundMinSize().width());
    int minHeight = qMax(brect.height(), qnGlobals->layoutBackgroundMinSize().height());

    qreal cellAspectRatio = qnGlobals->defaultLayoutCellAspectRatio();
    if (layout->hasCellAspectRatio()) {
        qreal cellWidth = 1.0 + layout->cellSpacing().width();
        qreal cellHeight = 1.0 / layout->cellAspectRatio() + layout->cellSpacing().height();
        cellAspectRatio = cellWidth / cellHeight;
    }

    qreal aspectRatio = sender()->property(uploadingImageARPropertyName).toReal();

    int w, h;
    qreal targetAspectRatio = aspectRatio / cellAspectRatio;
    if (targetAspectRatio >= 1.0) { // width is greater than height
        h = minHeight;
        w = qRound((qreal)h * targetAspectRatio);
        if (w > qnGlobals->layoutBackgroundMaxSize().width()) {
            w = qnGlobals->layoutBackgroundMaxSize().width();
            h = qRound((qreal)w / targetAspectRatio);
        }

    } else {
        w = minWidth;
        h = qRound((qreal)w / targetAspectRatio);
        if (h > qnGlobals->layoutBackgroundMaxSize().height()) {
            h = qnGlobals->layoutBackgroundMaxSize().height();
            w = qRound((qreal)h * targetAspectRatio);
        }
    }
    layout->setBackgroundSize(QSize(w, h));
}

void QnWorkbenchActionHandler::at_resources_saved(int status, const QnResourceList &resources, int handle) {
    Q_UNUSED(handle);

    if(status == 0)
        return;

    if (resources.isEmpty())
        return;

    QnResourceListDialog::exec(
                mainWindow(),
                resources,
                tr("Error"),
                tr("Could not save the following %n items to Enterprise Controller.", "", resources.size()),
                QDialogButtonBox::Ok
                );

}

void QnWorkbenchActionHandler::at_resource_deleted(const QnHTTPRawResponse& response, int handle) {
    Q_UNUSED(handle);

    if(response.status == 0)
        return;

    QMessageBox::critical(mainWindow(), tr("Could not delete resource"), tr("An error has occurred while trying to delete a resource from Enterprise Controller. \n\nError description: '%2'").arg(QLatin1String(response.errorString.data())));
}

void QnWorkbenchActionHandler::at_resources_statusSaved(int status, const QnResourceList &resources, const QList<int> &oldDisabledFlags) {
    if(status == 0 || resources.isEmpty())
        return;

    QnResourceListDialog::exec(
        mainWindow(),
        resources,
        tr("Error"),
        tr("Could not save changes made to the following %n resource(s).", "", resources.size()),
        QDialogButtonBox::Ok
    );

    for(int i = 0; i < resources.size(); i++)
        resources[i]->setDisabled(oldDisabledFlags[i]);
}

void QnWorkbenchActionHandler::at_panicWatcher_panicModeChanged() {
    action(Qn::TogglePanicModeAction)->setChecked(context()->instance<QnWorkbenchPanicWatcher>()->isPanicMode());
    //if (!action(Qn::TogglePanicModeAction)->isChecked()) {

    bool enabled = 
        context()->instance<QnWorkbenchScheduleWatcher>()->isScheduleEnabled() &&
        (accessController()->globalPermissions() & Qn::GlobalPanicPermission);
    action(Qn::TogglePanicModeAction)->setEnabled(enabled);

    //}
}

void QnWorkbenchActionHandler::at_scheduleWatcher_scheduleEnabledChanged() {
    // TODO: #Elric totally evil copypasta and hacky workaround.
    bool enabled = 
        context()->instance<QnWorkbenchScheduleWatcher>()->isScheduleEnabled() &&
        (accessController()->globalPermissions() & Qn::GlobalPanicPermission);

    action(Qn::TogglePanicModeAction)->setEnabled(enabled);
    if (!enabled)
        action(Qn::TogglePanicModeAction)->setChecked(false);
}

void QnWorkbenchActionHandler::at_togglePanicModeAction_toggled(bool checked) {
    QnMediaServerResourceList resources = resourcePool()->getResources().filtered<QnMediaServerResource>();

    foreach(QnMediaServerResourcePtr resource, resources)
    {
        bool isPanicMode = resource->getPanicMode() != QnMediaServerResource::PM_None;
        if(isPanicMode != checked) {
            QnMediaServerResource::PanicMode val = QnMediaServerResource::PM_None;
            if (checked)
                val = QnMediaServerResource::PM_User;
            resource->setPanicMode(val);
            connection()->saveAsync(resource, this, SLOT(at_resources_saved(int, const QnResourceList &, int)));
        }
    }
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
    if(!workbench()->item(Qn::ZoomedRole))
        action(Qn::ToggleTourModeAction)->setChecked(false);

    if (role == Qn::CentralRole && adjustVideoDialog()->isVisible())
    {
        QnWorkbenchItem *item = context()->workbench()->item(Qn::CentralRole);
        QnMediaResourceWidget* widget = dynamic_cast<QnMediaResourceWidget*> (context()->display()->widget(item));
        if (widget)
            adjustVideoDialog()->setWidget(widget);
    }
}

void QnWorkbenchActionHandler::at_whatsThisAction_triggered() {
    QWhatsThis::enterWhatsThisMode();
}

void QnWorkbenchActionHandler::at_escapeHotkeyAction_triggered() {
    if (action(Qn::ToggleTourModeAction)->isChecked())
        menu()->trigger(Qn::ToggleTourModeAction);
    else
        menu()->trigger(Qn::EffectiveMaximizeAction);
}

void QnWorkbenchActionHandler::at_clearCacheAction_triggered() {
    QnAppServerFileCache::clearLocalCache();
}

void QnWorkbenchActionHandler::at_messageBoxAction_triggered() {
    QString title = menu()->currentParameters(sender()).argument<QString>(Qn::TitleRole);
    QString text = menu()->currentParameters(sender()).argument<QString>(Qn::TextRole);
    if (text.isEmpty())
        text = title;

    QMessageBox::information(mainWindow(), title, text);
}

void QnWorkbenchActionHandler::at_browseUrlAction_triggered() {
    QString url = menu()->currentParameters(sender()).argument<QString>(Qn::UrlRole);
    if (url.isEmpty())
        return;

    QDesktopServices::openUrl(QUrl::fromUserInput(url));
}

void QnWorkbenchActionHandler::at_versionMismatchMessageAction_triggered() {
    QnWorkbenchVersionMismatchWatcher *watcher = context()->instance<QnWorkbenchVersionMismatchWatcher>();
    if(!watcher->hasMismatches())
        return;

    QnSoftwareVersion latestVersion = watcher->latestVersion();
    QnSoftwareVersion latestMsVersion = watcher->latestVersion(Qn::MediaServerComponent);

    // if some component is newer than the newest mediaserver, focus on its version
    if (QnWorkbenchVersionMismatchWatcher::versionMismatches(latestVersion, latestMsVersion))
        latestMsVersion = latestVersion;

    QString components;
    foreach(const QnVersionMismatchData &data, watcher->mismatchData()) {
        QString component;
        switch(data.component) {
        case Qn::ClientComponent:
            component = tr("Client v%1<br/>").arg(data.version.toString());
            break;
        case Qn::EnterpriseControllerComponent:
            component = tr("Enterprise Controller v%1<br/>").arg(data.version.toString());
            break;
        case Qn::MediaServerComponent: {
            QnMediaServerResourcePtr resource = data.resource.dynamicCast<QnMediaServerResource>();
            if(resource) {
                component = tr("Media Server v%1 at %2<br/>").arg(data.version.toString()).arg(QUrl(resource->getUrl()).host());
            } else {
                component = tr("Media Server v%1<br/>").arg(data.version.toString());
            }
        }
        default:
            break;
        }

        bool updateRequested = false;
        switch (data.component) {
        case Qn::MediaServerComponent:
            updateRequested = QnWorkbenchVersionMismatchWatcher::versionMismatches(data.version, latestMsVersion, true);
            break;
        case Qn::EnterpriseControllerComponent:
            updateRequested = QnWorkbenchVersionMismatchWatcher::versionMismatches(data.version, latestVersion);
            break;
        case Qn::ClientComponent:
            updateRequested = false;
            break;
        default:
            break;
        }

        if (updateRequested)
            component = QString(lit("<font color=\"%1\">%2</font>")).arg(qnGlobals->errorTextColor().name()).arg(component);
        
        components += component;
    }


    QString message = tr(
        "Some components of the system are not upgraded:<br/>"
        "<br/>"
        "%1"
        "<br/>"
        "Please upgrade all components to the latest version %2."
    ).arg(components).arg(latestMsVersion.toString());

    QnMessageBox::warning(mainWindow(), Qn::VersionMismatch_Help, tr("Version Mismatch"), message);
}

void QnWorkbenchActionHandler::at_versionMismatchWatcher_mismatchDataChanged() {
    menu()->trigger(Qn::VersionMismatchMessageAction);
}

void QnWorkbenchActionHandler::at_betaVersionMessageAction_triggered() {
    QMessageBox::warning(mainWindow(),
                         tr("Beta version"),
                         tr("You are running beta version of %1.")
                         .arg(QLatin1String(QN_APPLICATION_NAME)));
}

void QnWorkbenchActionHandler::at_queueAppRestartAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnSoftwareVersion version = parameters.hasArgument(Qn::SoftwareVersionRole)
            ? parameters.argument<QnSoftwareVersion>(Qn::SoftwareVersionRole)
            : QnSoftwareVersion();
    QUrl url = parameters.hasArgument(Qn::UrlRole)
            ? parameters.argument<QUrl>(Qn::UrlRole)
            : context()->user()
              ? QnAppServerConnectionFactory::defaultUrl()
              : QUrl();
    QByteArray auth = url.toEncoded();

    bool isInstalled;
    bool success = applauncher::isVersionInstalled(version, &isInstalled) == applauncher::api::ResultType::ok;
    if (success && isInstalled) {
        //TODO: #GDM wtf? whats up with order?
        if(context()->user()) { // TODO: #Elric factor out
            QnWorkbenchState state;
            workbench()->submit(state);

            QnWorkbenchStateHash states = qnSettings->userWorkbenchStates();
            states[context()->user()->getName()] = state;
            qnSettings->setUserWorkbenchStates(states);
        }

        //TODO: #GDM factor out
        if(!context()->instance<QnWorkbenchLayoutsHandler>()->closeAllLayouts(true)) {
            return;
        }
        menu()->trigger(Qn::ClearCameraSettingsAction);

        success = applauncher::restartClient(version, auth) == applauncher::api::ResultType::ok;
    }

    if (!success) {
        QMessageBox::critical(
                    mainWindow(),
                    tr("Launcher process is not found"),
                    tr("Cannot restart the client.\n"
                       "Please close the application and start it again using the shortcut in the start menu.")
                    );
        return;
    }
    qApp->exit(0);
    applauncher::scheduleProcessKill( QCoreApplication::applicationPid(), PROCESS_TERMINATE_TIMEOUT );
}

