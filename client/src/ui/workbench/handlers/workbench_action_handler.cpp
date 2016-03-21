#include "workbench_action_handler.h"

#include <cassert>

#include <QtCore/QProcess>

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include <QtGui/QDesktopServices>
#include <QtGui/QImage>
#include <QtWidgets/QWhatsThis>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QCheckBox>
#include <QtGui/QImageWriter>

#include <api/network_proxy_factory.h>
#include <api/global_settings.h>

#include <business/business_action_parameters.h>

#include <camera/resource_display.h>
#include <camera/cam_display.h>
#include <camera/camera_data_manager.h>

#include <client/client_connection_data.h>
#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>

#include <common/common_module.h>

#include <core/resource/resource.h>
#include <core/resource/resource_name.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource/file_processor.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/user_resource.h>

#include <nx_ec/dummy_handler.h>

#include <plugins/resource/archive/archive_stream_reader.h>
#include <plugins/resource/avi/avi_resource.h>
#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <recording/time_period_list.h>

#include <redass/redass_controller.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_parameter_types.h>
#include <ui/actions/action_target_provider.h>

#include <ui/dialogs/about_dialog.h>
#include <ui/dialogs/connection_testing_dialog.h>
#include <ui/dialogs/user_settings_dialog.h>
#include <ui/dialogs/resource_list_dialog.h>
#include <ui/dialogs/preferences_dialog.h>
#include <ui/dialogs/camera_addition_dialog.h>
#include <ui/dialogs/progress_dialog.h>
#include <ui/dialogs/business_rules_dialog.h>
#include <ui/dialogs/checkable_message_box.h>
#include <ui/dialogs/failover_priority_dialog.h>
#include <ui/dialogs/backup_cameras_dialog.h>
#include <ui/dialogs/layout_settings_dialog.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/file_dialog.h>
#include <ui/dialogs/camera_diagnostics_dialog.h>
#include <ui/dialogs/message_box.h>
#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/dialogs/media_file_settings_dialog.h>
#include <ui/dialogs/ping_dialog.h>
#include <ui/dialogs/system_administration_dialog.h>
#include <ui/dialogs/non_modal_dialog_constructor.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>

#include <ui/common/ui_resource_name.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/style/globals.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_synchronizer.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_resource.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_state_manager.h>
#include <ui/workbench/workbench_navigator.h>

#include <ui/workbench/handlers/workbench_layouts_handler.h>            //TODO: #GDM dependencies

#include <ui/workbench/watchers/workbench_user_watcher.h>
#include <ui/workbench/watchers/workbench_panic_watcher.h>
#include <ui/workbench/watchers/workbench_schedule_watcher.h>
#include <ui/workbench/watchers/workbench_update_watcher.h>
#include <ui/workbench/watchers/workbench_user_layout_count_watcher.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>
#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>
#include <ui/workbench/watchers/workbench_bookmarks_watcher.h>

#include <utils/app_server_image_cache.h>

#include <utils/applauncher_utils.h>
#include <utils/local_file_cache.h>
#include <utils/common/environment.h>
#include <utils/common/delete_later.h>
#include <utils/common/mime_data.h>
#include <utils/common/event_processors.h>
#include <utils/common/string.h>
#include <utils/common/time.h>
#include <utils/common/synctime.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/url.h>
#include <utils/email/email.h>
#include <utils/math/math.h>
#include <utils/network/http/httptypes.h>
#include <utils/aspect_ratio.h>
#include <utils/screen_manager.h>


#ifdef Q_OS_MACX
#include <utils/mac_utils.h>
#endif

#include <utils/common/app_info.h>

// TODO: #Elric remove this include
#include "../extensions/workbench_stream_synchronizer.h"

#include "core/resource/layout_item_data.h"
#include "ui/dialogs/adjust_video_dialog.h"
#include "ui/graphics/items/resource/resource_widget_renderer.h"
#include "ui/widgets/palette_widget.h"
#include "network/authutil.h"

namespace {
    const char* uploadingImageARPropertyName = "_qn_uploadingImageARPropertyName";
}

//!time that is given to process to exit. After that, applauncher (if present) will try to terminate it
static const quint32 PROCESS_TERMINATE_TIMEOUT = 15000;

// -------------------------------------------------------------------------- //
// QnWorkbenchActionHandler
// -------------------------------------------------------------------------- //
QnWorkbenchActionHandler::QnWorkbenchActionHandler(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_delayedDropGuard(false),
    m_tourTimer(new QTimer(this))
{
    connect(m_tourTimer,                                        SIGNAL(timeout()),                              this,   SLOT(at_tourTimer_timeout()));
    connect(context(),                                          SIGNAL(userChanged(const QnUserResourcePtr &)), this,   SLOT(at_context_userChanged(const QnUserResourcePtr &)), Qt::QueuedConnection);

    connect(workbench(),                                        SIGNAL(itemChanged(Qn::ItemRole)),              this,   SLOT(at_workbench_itemChanged(Qn::ItemRole)));
    connect(workbench(),                                        SIGNAL(cellSpacingChanged()),                   this,   SLOT(at_workbench_cellSpacingChanged()));
    connect(workbench(),                                        SIGNAL(currentLayoutChanged()),                 this,   SLOT(at_workbench_currentLayoutChanged()));

    connect(action(QnActions::MainMenuAction),                         SIGNAL(triggered()),    this,   SLOT(at_mainMenuAction_triggered()));
    connect(action(QnActions::OpenCurrentUserLayoutMenu),              SIGNAL(triggered()),    this,   SLOT(at_openCurrentUserLayoutMenuAction_triggered()));
    connect(action(QnActions::ShowcaseAction),                         SIGNAL(triggered()),    this,   SLOT(at_showcaseAction_triggered()));
    connect(action(QnActions::AboutAction),                            SIGNAL(triggered()),    this,   SLOT(at_aboutAction_triggered()));
    /* These actions may be activated via context menu. In this case the topmost event loop will be finishing and this somehow affects runModal method of NSSavePanel in MacOS.
     * File dialog execution will be failed. (see a comment in qcocoafiledialoghelper.mm)
     * To make dialogs work we're using queued connection here. */
    connect(action(QnActions::OpenFileAction),                         SIGNAL(triggered()),    this,   SLOT(at_openFileAction_triggered()));
    connect(action(QnActions::OpenLayoutAction),                       SIGNAL(triggered()),    this,   SLOT(at_openLayoutAction_triggered()));
    connect(action(QnActions::OpenFolderAction),                       SIGNAL(triggered()),    this,   SLOT(at_openFolderAction_triggered()));

    connect(action(QnActions::PreferencesGeneralTabAction),            SIGNAL(triggered()),    this,   SLOT(at_preferencesGeneralTabAction_triggered()));
    connect(action(QnActions::PreferencesLicensesTabAction),           SIGNAL(triggered()),    this,   SLOT(at_preferencesLicensesTabAction_triggered()));
    connect(action(QnActions::PreferencesSmtpTabAction),               SIGNAL(triggered()),    this,   SLOT(at_preferencesSmtpTabAction_triggered()));
    connect(action(QnActions::PreferencesNotificationTabAction),       SIGNAL(triggered()),    this,   SLOT(at_preferencesNotificationTabAction_triggered()));
    connect(action(QnActions::BusinessEventsAction),                   SIGNAL(triggered()),    this,   SLOT(at_businessEventsAction_triggered()));
    connect(action(QnActions::OpenBusinessRulesAction),                SIGNAL(triggered()),    this,   SLOT(at_openBusinessRulesAction_triggered()));
    connect(action(QnActions::OpenFailoverPriorityAction),             &QAction::triggered,    this,   &QnWorkbenchActionHandler::openFailoverPriorityDialog);
    connect(action(QnActions::OpenBackupCamerasAction),                &QAction::triggered,    this,   &QnWorkbenchActionHandler::openBackupCamerasDialog);
    connect(action(QnActions::OpenBookmarksSearchAction),              SIGNAL(triggered()),    this,   SLOT(at_openBookmarksSearchAction_triggered()));
    connect(action(QnActions::OpenBusinessLogAction),                  SIGNAL(triggered()),    this,   SLOT(at_openBusinessLogAction_triggered()));
    connect(action(QnActions::OpenAuditLogAction),                     SIGNAL(triggered()),    this,   SLOT(at_openAuditLogAction_triggered()));
    connect(action(QnActions::CameraListAction),                       SIGNAL(triggered()),    this,   SLOT(at_cameraListAction_triggered()));
    connect(action(QnActions::CameraListByServerAction),               SIGNAL(triggered()),    this,   SLOT(at_cameraListAction_triggered()));
    connect(action(QnActions::WebClientAction),                        SIGNAL(triggered()),    this,   SLOT(at_webClientAction_triggered()));
    connect(action(QnActions::WebClientActionSubMenu),                 SIGNAL(triggered()),    this,   SLOT(at_webClientAction_triggered()));
    connect(action(QnActions::SystemAdministrationAction),             SIGNAL(triggered()),    this,   SLOT(at_systemAdministrationAction_triggered()));
    connect(action(QnActions::SystemUpdateAction),                     SIGNAL(triggered()),    this,   SLOT(at_systemUpdateAction_triggered()));
    connect(action(QnActions::UserManagementAction),                   SIGNAL(triggered()),    this,   SLOT(at_userManagementAction_triggered()));
    connect(action(QnActions::NextLayoutAction),                       SIGNAL(triggered()),    this,   SLOT(at_nextLayoutAction_triggered()));
    connect(action(QnActions::PreviousLayoutAction),                   SIGNAL(triggered()),    this,   SLOT(at_previousLayoutAction_triggered()));
    connect(action(QnActions::OpenInLayoutAction),                     SIGNAL(triggered()),    this,   SLOT(at_openInLayoutAction_triggered()));
    connect(action(QnActions::OpenInCurrentLayoutAction),              SIGNAL(triggered()),    this,   SLOT(at_openInCurrentLayoutAction_triggered()));
    connect(action(QnActions::OpenInNewLayoutAction),                  SIGNAL(triggered()),    this,   SLOT(at_openInNewLayoutAction_triggered()));
    connect(action(QnActions::OpenInNewWindowAction),                  SIGNAL(triggered()),    this,   SLOT(at_openInNewWindowAction_triggered()));
    connect(action(QnActions::OpenSingleLayoutAction),                 SIGNAL(triggered()),    this,   SLOT(at_openLayoutsAction_triggered()));
    connect(action(QnActions::OpenMultipleLayoutsAction),              SIGNAL(triggered()),    this,   SLOT(at_openLayoutsAction_triggered()));
    connect(action(QnActions::OpenAnyNumberOfLayoutsAction),           SIGNAL(triggered()),    this,   SLOT(at_openLayoutsAction_triggered()));
    connect(action(QnActions::OpenLayoutsInNewWindowAction),           SIGNAL(triggered()),    this,   SLOT(at_openLayoutsInNewWindowAction_triggered()));
    connect(action(QnActions::OpenCurrentLayoutInNewWindowAction),     SIGNAL(triggered()),    this,   SLOT(at_openCurrentLayoutInNewWindowAction_triggered()));
    connect(action(QnActions::OpenNewTabAction),                       SIGNAL(triggered()),    this,   SLOT(at_openNewTabAction_triggered()));
    connect(action(QnActions::OpenNewWindowAction),                    SIGNAL(triggered()),    this,   SLOT(at_openNewWindowAction_triggered()));
    connect(action(QnActions::UserSettingsAction),                     SIGNAL(triggered()),    this,   SLOT(at_userSettingsAction_triggered()));
    connect(action(QnActions::MediaFileSettingsAction),                &QAction::triggered,    this,   &QnWorkbenchActionHandler::at_mediaFileSettingsAction_triggered);
    connect(action(QnActions::CameraIssuesAction),                     SIGNAL(triggered()),    this,   SLOT(at_cameraIssuesAction_triggered()));
    connect(action(QnActions::CameraBusinessRulesAction),              SIGNAL(triggered()),    this,   SLOT(at_cameraBusinessRulesAction_triggered()));
    connect(action(QnActions::CameraDiagnosticsAction),                SIGNAL(triggered()),    this,   SLOT(at_cameraDiagnosticsAction_triggered()));
    connect(action(QnActions::LayoutSettingsAction),                   SIGNAL(triggered()),    this,   SLOT(at_layoutSettingsAction_triggered()));
    connect(action(QnActions::CurrentLayoutSettingsAction),            SIGNAL(triggered()),    this,   SLOT(at_currentLayoutSettingsAction_triggered()));
    connect(action(QnActions::ServerAddCameraManuallyAction),          SIGNAL(triggered()),    this,   SLOT(at_serverAddCameraManuallyAction_triggered()));
    connect(action(QnActions::PingAction),                             SIGNAL(triggered()),    this,   SLOT(at_pingAction_triggered()));
    connect(action(QnActions::ServerLogsAction),                       SIGNAL(triggered()),    this,   SLOT(at_serverLogsAction_triggered()));
    connect(action(QnActions::ServerIssuesAction),                     SIGNAL(triggered()),    this,   SLOT(at_serverIssuesAction_triggered()));
    connect(action(QnActions::OpenInFolderAction),                     SIGNAL(triggered()),    this,   SLOT(at_openInFolderAction_triggered()));
    connect(action(QnActions::DeleteFromDiskAction),                   SIGNAL(triggered()),    this,   SLOT(at_deleteFromDiskAction_triggered()));
    connect(action(QnActions::RemoveLayoutItemAction),                 SIGNAL(triggered()),    this,   SLOT(at_removeLayoutItemAction_triggered()));
    connect(action(QnActions::RemoveFromServerAction),                 SIGNAL(triggered()),    this,   SLOT(at_removeFromServerAction_triggered()));
    connect(action(QnActions::NewUserAction),                          SIGNAL(triggered()),    this,   SLOT(at_newUserAction_triggered()));
    connect(action(QnActions::RenameResourceAction),                   SIGNAL(triggered()),    this,   SLOT(at_renameAction_triggered()));
    connect(action(QnActions::DropResourcesAction),                    SIGNAL(triggered()),    this,   SLOT(at_dropResourcesAction_triggered()));
    connect(action(QnActions::DelayedDropResourcesAction),             SIGNAL(triggered()),    this,   SLOT(at_delayedDropResourcesAction_triggered()));
    connect(action(QnActions::InstantDropResourcesAction),             SIGNAL(triggered()),    this,   SLOT(at_instantDropResourcesAction_triggered()));
    connect(action(QnActions::DropResourcesIntoNewLayoutAction),       SIGNAL(triggered()),    this,   SLOT(at_dropResourcesIntoNewLayoutAction_triggered()));
    connect(action(QnActions::MoveCameraAction),                       SIGNAL(triggered()),    this,   SLOT(at_moveCameraAction_triggered()));
    connect(action(QnActions::AdjustVideoAction),                      SIGNAL(triggered()),    this,   SLOT(at_adjustVideoAction_triggered()));
    connect(action(QnActions::ExitAction),                             &QAction::triggered,    this,   &QnWorkbenchActionHandler::closeApplication);
    connect(action(QnActions::ThumbnailsSearchAction),                 SIGNAL(triggered()),    this,   SLOT(at_thumbnailsSearchAction_triggered()));
    connect(action(QnActions::SetCurrentLayoutItemSpacing0Action),     SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutItemSpacing0Action_triggered()));
    connect(action(QnActions::SetCurrentLayoutItemSpacing10Action),    SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutItemSpacing10Action_triggered()));
    connect(action(QnActions::SetCurrentLayoutItemSpacing20Action),    SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutItemSpacing20Action_triggered()));
    connect(action(QnActions::SetCurrentLayoutItemSpacing30Action),    SIGNAL(triggered()),    this,   SLOT(at_setCurrentLayoutItemSpacing30Action_triggered()));
    connect(action(QnActions::CreateZoomWindowAction),                 SIGNAL(triggered()),    this,   SLOT(at_createZoomWindowAction_triggered()));
    connect(action(QnActions::Rotate0Action),                          &QAction::triggered,    this,   [this] { rotateItems(0); });
    connect(action(QnActions::Rotate90Action),                         &QAction::triggered,    this,   [this] { rotateItems(90); });
    connect(action(QnActions::Rotate180Action),                        &QAction::triggered,    this,   [this] { rotateItems(180); });
    connect(action(QnActions::Rotate270Action),                        &QAction::triggered,    this,   [this] { rotateItems(270); });
    connect(action(QnActions::RadassAutoAction),                       &QAction::triggered,    this,   [this] { setResolutionMode(Qn::AutoResolution); });
    connect(action(QnActions::RadassLowAction),                        &QAction::triggered,    this,   [this] { setResolutionMode(Qn::LowResolution); });
    connect(action(QnActions::RadassHighAction),                       &QAction::triggered,    this,   [this] { setResolutionMode(Qn::HighResolution); });
    connect(action(QnActions::SetAsBackgroundAction),                  SIGNAL(triggered()),    this,   SLOT(at_setAsBackgroundAction_triggered()));
    connect(action(QnActions::WhatsThisAction),                        SIGNAL(triggered()),    this,   SLOT(at_whatsThisAction_triggered()));
    connect(action(QnActions::EscapeHotkeyAction),                     SIGNAL(triggered()),    this,   SLOT(at_escapeHotkeyAction_triggered()));
    connect(action(QnActions::MessageBoxAction),                       SIGNAL(triggered()),    this,   SLOT(at_messageBoxAction_triggered()));
    connect(action(QnActions::BrowseUrlAction),                        SIGNAL(triggered()),    this,   SLOT(at_browseUrlAction_triggered()));
    connect(action(QnActions::VersionMismatchMessageAction),           SIGNAL(triggered()),    this,   SLOT(at_versionMismatchMessageAction_triggered()));
    connect(action(QnActions::BetaVersionMessageAction),               SIGNAL(triggered()),    this,   SLOT(at_betaVersionMessageAction_triggered()));
    connect(action(QnActions::AllowStatisticsReportMessageAction),     &QAction::triggered,    this,   [this] { checkIfStatisticsReportAllowed(); });

    /* Qt::QueuedConnection is important! See QnPreferencesDialog::canApplyChanges() for details. */
    connect(action(QnActions::QueueAppRestartAction),                  SIGNAL(triggered()),    this,   SLOT(at_queueAppRestartAction_triggered()), Qt::QueuedConnection);
    connect(action(QnActions::SelectTimeServerAction),                 SIGNAL(triggered()),    this,   SLOT(at_selectTimeServerAction_triggered()));

    connect(action(QnActions::TogglePanicModeAction),                  SIGNAL(toggled(bool)),  this,   SLOT(at_togglePanicModeAction_toggled(bool)));
    connect(action(QnActions::ToggleTourModeAction),                   SIGNAL(toggled(bool)),  this,   SLOT(at_toggleTourAction_toggled(bool)));
    //connect(context()->instance<QnWorkbenchPanicWatcher>(),     SIGNAL(panicModeChanged()), this, SLOT(at_panicWatcher_panicModeChanged()));
    connect(context()->instance<QnWorkbenchScheduleWatcher>(),  SIGNAL(scheduleEnabledChanged()),   this,   SLOT(at_scheduleWatcher_scheduleEnabledChanged()));

    /* Connect through lambda to handle forced parameter. */
    connect(action(QnActions::DelayedForcedExitAction),                &QAction::triggered,    this,   [this] {  closeApplication(true);    }, Qt::QueuedConnection);

    connect(action(QnActions::BeforeExitAction),  &QAction::triggered, this, &QnWorkbenchActionHandler::at_beforeExitAction_triggered);

    /* Run handlers that update state. */
    //at_panicWatcher_panicModeChanged();
    at_scheduleWatcher_scheduleEnabledChanged();
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

    deleteDialogs();
}

ec2::AbstractECConnectionPtr QnWorkbenchActionHandler::connection2() const {
    return QnAppServerConnectionFactory::getConnection2();
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

#ifndef DESKTOP_CAMERA_DEBUG
    if (resource->hasFlags(Qn::desktop_camera))
        return;
#endif

    {
        //TODO: #GDM #Common refactor duplicated code
        bool isServer = resource->hasFlags(Qn::server);
        bool isMediaResource = resource->hasFlags(Qn::media);
        bool isLocalResource = resource->hasFlags(Qn::url | Qn::local | Qn::media)
                && !resource->getUrl().startsWith(QnLayoutFileStorageResource::layoutPrefix());
        bool isExportedLayout = snapshotManager()->isFile(layout);

        bool allowed = isServer || isMediaResource;
        bool forbidden = isExportedLayout && (isServer || isLocalResource);
        if(!allowed || forbidden)
            return;
    }

    QnLayoutItemData data;
    data.resource.id = resource->getId();
    data.resource.path = resource->getUniqueId();
    data.uuid = QnUuid::createUuid();
    data.flags = Qn::PendingGeometryAdjustment;
    data.zoomRect = params.zoomWindow;
    data.zoomTargetUuid = params.zoomUuid;

    if (!qFuzzyIsNull(params.rotation)) {
        data.rotation = params.rotation;
    }
    else {
        QString forcedRotation = resource->getProperty(QnMediaResource::rotationKey());
        if (!forcedRotation.isEmpty())
            data.rotation = forcedRotation.toInt();
    }
    data.contrastParams = params.contrastParams;
    data.dewarpingParams = params.dewarpingParams;
    data.dataByRole[Qn::ItemTimeRole] = params.time;
    if(params.frameDistinctionColor.isValid())
        data.dataByRole[Qn::ItemFrameDistinctionColorRole] = params.frameDistinctionColor;
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
    arguments << lit("--no-single-application");
    arguments << lit("--no-version-mismatch-check");

    if (context()->user()) {
        arguments << lit("--auth");
        arguments << QString::fromUtf8(QnAppServerConnectionFactory::url().toEncoded());
    }

    if(mainWindow()) {
        int screen = context()->instance<QnScreenManager>()->nextFreeScreen();
        arguments << QLatin1String("--screen") << QString::number(screen);
    }

    if (qnRuntime->isDevMode())
        arguments << lit("--dev-mode-key=razrazraz");

    qDebug() << "Starting new instance with args" << arguments;

#ifdef Q_OS_MACX
    mac_startDetached(qApp->applicationFilePath(), arguments);
#else
    QProcess::startDetached(qApp->applicationFilePath(), arguments);
#endif
}

void QnWorkbenchActionHandler::rotateItems(int degrees){
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();
    if(!widgets.empty()) {
        foreach(const QnResourceWidget *widget, widgets)
            widget->item()->setRotation(degrees);
    }
}

void QnWorkbenchActionHandler::setResolutionMode(Qn::ResolutionMode resolutionMode) {
    if (qnRedAssController)
        qnRedAssController->setMode(resolutionMode);
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

QnSystemAdministrationDialog *QnWorkbenchActionHandler::systemAdministrationDialog() const {
    return m_systemAdministrationDialog.data();
}

void QnWorkbenchActionHandler::submitDelayedDrops() {
    if (m_delayedDropGuard)
        return;

    if(!context()->user())
        return;

    if (!context()->workbench()->currentLayout()->resource())
        return;


    QN_SCOPED_VALUE_ROLLBACK(&m_delayedDropGuard, true);

    foreach(const QnMimeData &data, m_delayedDrops) {
        QMimeData mimeData;
        data.toMimeData(&mimeData);

        QnResourceList resources = QnWorkbenchResource::deserializeResources(&mimeData);
        QnLayoutResourceList layouts = resources.filtered<QnLayoutResource>();
        if (!layouts.isEmpty()){
            workbench()->clear();
            menu()->trigger(QnActions::OpenAnyNumberOfLayoutsAction, layouts);
        } else {
            menu()->trigger(QnActions::OpenInCurrentLayoutAction, resources);
        }
    }

    m_delayedDrops.clear();
}

void QnWorkbenchActionHandler::submitInstantDrop() {

    if (QnResourceDiscoveryManager::instance()->state() == QnResourceDiscoveryManager::InitialSearch) {
        // local resources are not ready yet
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
            menu()->trigger(QnActions::OpenAnyNumberOfLayoutsAction, layouts);
        } else {
            menu()->trigger(QnActions::OpenInCurrentLayoutAction, resources);
        }
    }
    m_instantDrops.clear();
}



// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnWorkbenchActionHandler::at_context_userChanged(const QnUserResourcePtr &user) {
	if (qnRuntime->isDesktopMode())
    {
		if (user)
	        context()->instance<QnWorkbenchUpdateWatcher>()->start();
        else
            context()->instance<QnWorkbenchUpdateWatcher>()->stop();
	}

    if (!user)
        return;

    // we should not change state when using "Open in New Window"
    if (m_delayedDrops.isEmpty() && !qnRuntime->isVideoWallMode() && !qnRuntime->isActiveXMode()) {
        QnWorkbenchState state = qnSettings->userWorkbenchStates().value(user->getName());
        workbench()->update(state);

        /* Delete orphaned layouts. */
        foreach(const QnLayoutResourcePtr &layout, context()->resourcePool()->getResourcesWithParentId(QnUuid()).filtered<QnLayoutResource>())
            if(snapshotManager()->isLocal(layout) && !snapshotManager()->isFile(layout))
                resourcePool()->removeResource(layout);
    }

    /* Sometimes we get here when 'New Layout' has already been added. But all user's layouts must be created AFTER this method.
    * Otherwise the user will see uncreated layouts in layout selection menu.
    * As temporary workaround we can just remove that layouts. */
    // TODO: #dklychkov Do not create new empty layout before this method end. See: at_openNewTabAction_triggered()
    if (user && !qnRuntime->isActiveXMode()) {
        foreach(const QnLayoutResourcePtr &layout, context()->resourcePool()->getResourcesWithParentId(user->getId()).filtered<QnLayoutResource>()) {
            if(snapshotManager()->isLocal(layout) && !snapshotManager()->isFile(layout))
                resourcePool()->removeResource(layout);
        }
    }

    /* Close all other layouts. */
    foreach(QnWorkbenchLayout *layout, workbench()->layouts()) {
        QnLayoutResourcePtr resource = layout->resource();
        if(resource->getParentId() != user->getId())
            workbench()->removeLayout(layout);
    }

    if(workbench()->layouts().empty())
        menu()->trigger(QnActions::OpenNewTabAction);

    submitDelayedDrops();
}

void QnWorkbenchActionHandler::at_workbench_cellSpacingChanged() {
    qreal value = workbench()->currentLayout()->cellSpacing().width();

    if (qFuzzyCompare(0.0, value))
        action(QnActions::SetCurrentLayoutItemSpacing0Action)->setChecked(true);
    else if (qFuzzyCompare(0.2, value))
        action(QnActions::SetCurrentLayoutItemSpacing20Action)->setChecked(true);
    else if (qFuzzyCompare(0.3, value))
        action(QnActions::SetCurrentLayoutItemSpacing30Action)->setChecked(true);
    else
        action(QnActions::SetCurrentLayoutItemSpacing10Action)->setChecked(true); //default value
}

void QnWorkbenchActionHandler::at_workbench_currentLayoutChanged() {
    action(QnActions::RadassAutoAction)->setChecked(true);
    if (qnRedAssController)
        qnRedAssController->setMode(Qn::AutoResolution);
}

void QnWorkbenchActionHandler::at_mainMenuAction_triggered() {
    m_mainMenu = menu()->newMenu(Qn::MainScope, mainWindow());

    action(QnActions::MainMenuAction)->setMenu(m_mainMenu.data());
}

void QnWorkbenchActionHandler::at_openCurrentUserLayoutMenuAction_triggered() {
    if (qnRuntime->isVideoWallMode() || qnRuntime->isActiveXMode())
        return;

    m_currentUserLayoutsMenu = menu()->newMenu(QnActions::OpenCurrentUserLayoutMenu, Qn::TitleBarScope);

    action(QnActions::OpenCurrentUserLayoutMenu)->setMenu(m_currentUserLayoutsMenu.data());
}

void QnWorkbenchActionHandler::at_layoutCountWatcher_layoutCountChanged() {
    action(QnActions::OpenCurrentUserLayoutMenu)->setEnabled(context()->instance<QnWorkbenchUserLayoutCountWatcher>()->layoutCount() > 0);
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
        QHash<QnUuid, QnLayoutItemData> itemDataByUuid;
        foreach(const QnResourceWidget *widget, widgets) {
            QnLayoutItemData data = widget->item()->data();
            itemDataByUuid[data.uuid] = data;
        }

        /* Generate new UUIDs. */
        for(QHash<QnUuid, QnLayoutItemData>::iterator pos = itemDataByUuid.begin(); pos != itemDataByUuid.end(); pos++)
            pos->uuid = QnUuid::createUuid();

        /* Update cross-references. */
        for(QHash<QnUuid, QnLayoutItemData>::iterator pos = itemDataByUuid.begin(); pos != itemDataByUuid.end(); pos++)
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
                if (!widget)
                    continue;

                float aspectRatio = widget->visualChannelAspectRatio();
                if (aspectRatio <= 0)
                    continue;

                midAspectRatio += aspectRatio;
                ++count;
            }
        }

        if (count > 0) {
            midAspectRatio /= count;
            QnAspectRatio cellAspectRatio = QnAspectRatio::closestStandardRatio(midAspectRatio);
            layout->setCellAspectRatio(cellAspectRatio.toFloat());
        } else if (workbenchLayout->items().size() > 1) {
            layout->setCellAspectRatio(qnGlobals->defaultLayoutCellAspectRatio());
        }
    }
}

void QnWorkbenchActionHandler::at_openInCurrentLayoutAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    parameters.setArgument(Qn::LayoutResourceRole, workbench()->currentLayout()->resource());
    menu()->trigger(QnActions::OpenInLayoutAction, parameters);
}

void QnWorkbenchActionHandler::at_openInNewLayoutAction_triggered() {
    menu()->trigger(QnActions::OpenNewTabAction);
    menu()->trigger(QnActions::OpenInCurrentLayoutAction, menu()->currentParameters(sender()));
}

void QnWorkbenchActionHandler::at_openInNewWindowAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    parameters.setArgument(Qn::LayoutResourceRole, workbench()->currentLayout()->resource());

    QnResourceList filtered;
    foreach (const QnResourcePtr &resource, parameters.resources()) {
        if (resource->hasFlags(Qn::media) || resource->hasFlags(Qn::server))
            filtered << resource;
    }
    if (filtered.isEmpty())
        return;

    openResourcesInNewWindow(filtered);
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
        /* Explicit set that we do not control videowall through this layout */
        layout->setData(Qn::VideoWallItemGuidRole, qVariantFromValue(QnUuid()));

        workbench()->setCurrentLayout(layout);
    }
}

void QnWorkbenchActionHandler::at_openLayoutsInNewWindowAction_triggered() {
    // TODO: #GDM #Common this won't work for layouts that are not saved. (de)serialization of layouts is not implemented.
    QnLayoutResourceList layouts = menu()->currentParameters(sender()).resources().filtered<QnLayoutResource>();
    if(layouts.isEmpty())
        return;
    openResourcesInNewWindow(layouts);
}

void QnWorkbenchActionHandler::at_openCurrentLayoutInNewWindowAction_triggered() {
    menu()->trigger(QnActions::OpenLayoutsInNewWindowAction, workbench()->currentLayout()->resource());
}

void QnWorkbenchActionHandler::at_openNewTabAction_triggered() {
    QnWorkbenchLayout *layout = new QnWorkbenchLayout(this);

    layout->setName(generateUniqueLayoutName(context()->user(), tr("New Layout"), tr("New Layout %1")));

    workbench()->addLayout(layout);
    workbench()->setCurrentLayout(layout);
}

void QnWorkbenchActionHandler::at_openNewWindowAction_triggered() {
    openNewWindow(QStringList());
}

void QnWorkbenchActionHandler::at_cameraListChecked(int status, const QnCameraListReply& reply, int handle)
{
    if (!m_awaitingMoveCameras.contains(handle))
        return;
    QnVirtualCameraResourceList modifiedResources = m_awaitingMoveCameras.value(handle).cameras;
    QnMediaServerResourcePtr server = m_awaitingMoveCameras.value(handle).dstServer;
    m_awaitingMoveCameras.remove(handle);

    if (status != 0) {
        QnResourceListDialog::exec(
            mainWindow(),
            modifiedResources,
            Qn::MainWindow_Tree_DragCameras_Help,
            tr("Error"),
            QnDeviceDependentStrings::getNameFromSet(
                QnCameraDeviceStringSet(
                    tr("Cannot move these %n devices to server %1. Server is unresponsive.", "", modifiedResources.size()),
                    tr("Cannot move these %n cameras to server %1. Server is unresponsive.", "", modifiedResources.size()),
                    tr("Cannot move these %n I/O modules to server %1. Server is unresponsive.", "", modifiedResources.size())
                ),
                modifiedResources
            ).arg(server->getName()),
            QDialogButtonBox::Ok
            );
        return;
    }

    QnVirtualCameraResourceList errorResources; // TODO: #Elric check server cameras
    for (auto itr = modifiedResources.begin(); itr != modifiedResources.end();) {
        if (!reply.uniqueIdList.contains((*itr)->getUniqueId())) {
            errorResources << *itr;
            itr = modifiedResources.erase(itr);
        } else {
            ++itr;
        }
    }

    if(!errorResources.empty()) {
        QDialogButtonBox::StandardButton result =
            QnResourceListDialog::exec(
                mainWindow(),
                errorResources,
                Qn::MainWindow_Tree_DragCameras_Help,
                tr("Error"),
                QnDeviceDependentStrings::getNameFromSet(
                QnCameraDeviceStringSet(
                        tr("Server %1 is unable to find and access these %n devices. Are you sure you would like to move them?", "", errorResources.size()),
                        tr("Server %1 is unable to find and access these %n cameras. Are you sure you would like to move them?", "", errorResources.size()),
                        tr("Server %1 is unable to find and access these %n I/O modules. Are you sure you would like to move them?", "", errorResources.size())
                    ),
                    modifiedResources
                ).arg(server->getName()),
                QDialogButtonBox::Yes | QDialogButtonBox::No
                );
        /* If user is sure, return invalid cameras back to list. */
        if (result == QDialogButtonBox::Yes)
            modifiedResources << errorResources;
    }

    const QnUuid serverId = server->getId();
    qnResourcesChangesManager->saveCameras(modifiedResources, [serverId](const QnVirtualCameraResourcePtr &camera) {
        camera->setPreferedServerId(serverId);
    });

    qnResourcesChangesManager->saveCamerasCore(modifiedResources, [serverId](const QnVirtualCameraResourcePtr &camera) {
        camera->setParentId(serverId);
    });
}

void QnWorkbenchActionHandler::at_moveCameraAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnResourceList resources = parameters.resources();
    QnMediaServerResourcePtr server = parameters.argument<QnMediaServerResourcePtr>(Qn::MediaServerResourceRole);
    if(!server)
        return;
    QnVirtualCameraResourceList resourcesToMove;

    foreach(const QnResourcePtr &resource, resources) {
        if(resource->getParentId() == server->getId())
            continue; /* Moving resource into its owner does nothing. */

        QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
        if(!camera)
            continue;

        resourcesToMove.push_back(camera);
    }
    if (!resourcesToMove.isEmpty()) {
        int handle = server->apiConnection()->checkCameraList(resourcesToMove, this, SLOT(at_cameraListChecked(int, const QnCameraListReply &, int)));
        m_awaitingMoveCameras.insert(handle, CameraMovingInfo(resourcesToMove, server));
    }
}

void QnWorkbenchActionHandler::at_dropResourcesAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnResourceList resources = parameters.resources();
    QnLayoutResourceList layouts = resources.filtered<QnLayoutResource>();
    foreach(QnLayoutResourcePtr r, layouts)
        resources.removeOne(r);

    QnVideoWallResourceList videowalls = resources.filtered<QnVideoWallResource>();
    foreach(QnVideoWallResourcePtr r, videowalls)
        resources.removeOne(r);

    if (workbench()->currentLayout()->resource()->locked() &&
            !resources.empty() &&
            layouts.empty() &&
            videowalls.empty()) {
        QnGraphicsMessageBox::information(tr("Layout is locked and cannot be changed."));
        return;
    }

    if (!resources.empty()) {
        parameters.setResources(resources);
        if (menu()->canTrigger(QnActions::OpenInCurrentLayoutAction, parameters)) {
            menu()->trigger(QnActions::OpenInCurrentLayoutAction, parameters);
        } else {
            QnLayoutResourcePtr layout = workbench()->currentLayout()->resource();
            if (layout->hasFlags(Qn::url | Qn::local | Qn::layout)) {
                bool hasLocal = false;
                foreach (const QnResourcePtr &resource, resources) {
                    //TODO: #GDM #Common refactor duplicated code
                    hasLocal |= resource->hasFlags(Qn::url | Qn::local | Qn::media)
                            && !resource->getUrl().startsWith(QnLayoutFileStorageResource::layoutPrefix());
                    if (hasLocal)
                        break;
                }
                if (hasLocal)
                    QnMessageBox::warning(mainWindow(),
                                         tr("Cannot add item"),
                                         tr("Cannot add a local file to Multi-Video"));
            }
        }
    }

    if(!layouts.empty())
        menu()->trigger(QnActions::OpenAnyNumberOfLayoutsAction, layouts);

    if (!videowalls.empty())
        menu()->trigger(QnActions::OpenVideoWallsReviewAction, videowalls);
}

void QnWorkbenchActionHandler::at_dropResourcesIntoNewLayoutAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnLayoutResourceList layouts = parameters.resources().filtered<QnLayoutResource>();
    QnVideoWallResourceList videowalls = parameters.resources().filtered<QnVideoWallResource>();

    if(layouts.empty() && (videowalls.size() != parameters.resources().size())) /* There are some media in the drop, open new layout. */
        menu()->trigger(QnActions::OpenNewTabAction);

    menu()->trigger(QnActions::DropResourcesAction, parameters);
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

    QStringList files = QnFileDialog::getOpenFileNames(mainWindow(),
                                                       tr("Open File"),
                                                       QString(),
                                                       filters.join(lit(";;")),
                                                       0,
                                                       QnCustomFileDialog::fileDialogOptions());

    if (!files.isEmpty())
        menu()->trigger(QnActions::DropResourcesAction, addToResourcePool(files));
}

void QnWorkbenchActionHandler::at_openLayoutAction_triggered() {
    QStringList filters;
    filters << tr("All Supported (*.layout)");
    filters << tr("Layouts (*.layout)");
    filters << tr("All files (*.*)");

    QString fileName = QnFileDialog::getOpenFileName(mainWindow(),
                                                     tr("Open File"),
                                                     QString(),
                                                     filters.join(lit(";;")),
                                                     0,
                                                     QnCustomFileDialog::fileDialogOptions());

    if(!fileName.isEmpty())
        menu()->trigger(QnActions::DropResourcesAction, addToResourcePool(fileName).filtered<QnLayoutResource>());
}

void QnWorkbenchActionHandler::at_openFolderAction_triggered() {
    QString dirName = QnFileDialog::getExistingDirectory(mainWindow(),
                                                        tr("Select folder..."),
                                                        QString(),
                                                        QnCustomFileDialog::directoryDialogOptions());

    if(!dirName.isEmpty())
        menu()->trigger(QnActions::DropResourcesAction, addToResourcePool(dirName));
}

void QnWorkbenchActionHandler::openLayoutSettingsDialog(const QnLayoutResourcePtr &layout) {
    if(!layout)
        return;

    if(!accessController()->hasPermissions(layout, Qn::EditLayoutSettingsPermission))
        return;

    QScopedPointer<QnLayoutSettingsDialog> dialog(new QnLayoutSettingsDialog(mainWindow()));
    dialog->setWindowModality(Qt::ApplicationModal);
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

void QnWorkbenchActionHandler::openFailoverPriorityDialog() {
    QScopedPointer<QnFailoverPriorityDialog> dialog(new QnFailoverPriorityDialog(mainWindow()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::openBackupCamerasDialog() {
    QScopedPointer<QnBackupCamerasDialog> dialog(new QnBackupCamerasDialog(mainWindow()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
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
    QScopedPointer<QnPreferencesDialog> dialog(new QnPreferencesDialog(mainWindow()));
    dialog->setCurrentPage(QnPreferencesDialog::GeneralPage);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_preferencesLicensesTabAction_triggered() {
    QnNonModalDialogConstructor<QnSystemAdministrationDialog> dialogConstructor(m_systemAdministrationDialog, mainWindow());
    systemAdministrationDialog()->setCurrentPage(QnSystemAdministrationDialog::LicensesPage);
}

void QnWorkbenchActionHandler::at_preferencesSmtpTabAction_triggered() {
    QnNonModalDialogConstructor<QnSystemAdministrationDialog> dialogConstructor(m_systemAdministrationDialog, mainWindow());
    systemAdministrationDialog()->setCurrentPage(QnSystemAdministrationDialog::SmtpPage);
}

void QnWorkbenchActionHandler::at_preferencesNotificationTabAction_triggered() {
    QScopedPointer<QnPreferencesDialog> dialog(new QnPreferencesDialog(mainWindow()));
    dialog->setCurrentPage(QnPreferencesDialog::NotificationsPage);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_businessEventsAction_triggered() {
    menu()->trigger(QnActions::OpenBusinessRulesAction);
}

void QnWorkbenchActionHandler::at_openBusinessRulesAction_triggered() {
    QnNonModalDialogConstructor<QnBusinessRulesDialog> dialogConstructor(m_businessRulesDialog, mainWindow());

    QString filter;
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnVirtualCameraResourceList cameras = parameters.resources().filtered<QnVirtualCameraResource>();
    if (!cameras.isEmpty()) {
        foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
            filter += camera->getPhysicalId(); //getUniqueId() cannot be used here --gdm
        }
    }
    businessRulesDialog()->setFilter(filter);
}

void QnWorkbenchActionHandler::at_webClientAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnMediaServerResourcePtr server = parameters.resource().dynamicCast<QnMediaServerResource>();
    if (!server)
        /* If target server is not provided, open the server we are currently connected to. */
        server = qnCommon->currentServer();

    if (!server)
        return;

    QUrl url(server->getApiUrl());
    url.setUserName(QString());
    url.setPassword(QString());
    url.setScheme(lit("http"));
    url.setPath(lit("/static/index.html"));
    url = QnNetworkProxyFactory::instance()->urlToResource(url, server, lit("proxy"));
    QDesktopServices::openUrl(url);
}

void QnWorkbenchActionHandler::at_systemAdministrationAction_triggered() {
    QnNonModalDialogConstructor<QnSystemAdministrationDialog> dialogConstructor(m_systemAdministrationDialog, mainWindow());
    systemAdministrationDialog()->setCurrentPage(QnSystemAdministrationDialog::GeneralPage);
}

void QnWorkbenchActionHandler::at_systemUpdateAction_triggered() {
    QnNonModalDialogConstructor<QnSystemAdministrationDialog> dialogConstructor(m_systemAdministrationDialog, mainWindow());
    systemAdministrationDialog()->setCurrentPage(QnSystemAdministrationDialog::UpdatesPage);
}

void QnWorkbenchActionHandler::at_userManagementAction_triggered() {
    QnNonModalDialogConstructor<QnSystemAdministrationDialog> dialogConstructor(m_systemAdministrationDialog, mainWindow());
    systemAdministrationDialog()->setCurrentPage(QnSystemAdministrationDialog::UserManagement);
}

qint64 QnWorkbenchActionHandler::getFirstBookmarkTimeMs()
{
    static const qint64 kOneDayOffsetMs = 60 * 60 * 24 * 1000;
    static const qint64 kOneYearOffsetMs = kOneDayOffsetMs *  365;
    const auto nowMs = qnSyncTime->currentMSecsSinceEpoch();
    const auto bookmarksWatcher = context()->instance<QnWorkbenchBookmarksWatcher>();
    const auto firstBookmarkUtcTimeMs = bookmarksWatcher->firstBookmarkUtcTimeMs();
    const bool firstTimeIsNotKnown = (firstBookmarkUtcTimeMs == QnWorkbenchBookmarksWatcher::kUndefinedTime);
    return (firstTimeIsNotKnown ? nowMs - kOneYearOffsetMs : firstBookmarkUtcTimeMs);
}

void QnWorkbenchActionHandler::at_openBookmarksSearchAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    // If time window is specified then set it
    const auto nowMs = qnSyncTime->currentMSecsSinceEpoch();

    const auto filterText = (parameters.hasArgument(Qn::BookmarkTagRole)
        ? parameters.argument(Qn::BookmarkTagRole).toString() : QString());

    const auto timelineWindow = parameters.argument<QnTimePeriod>(Qn::ItemSliderWindowRole);
    const auto endTimeMs = (timelineWindow.isValid()
        ? (timelineWindow.isInfinite() ? nowMs : timelineWindow.endTimeMs()) : nowMs);

    const auto startTimeMs(timelineWindow.isValid()
        ? timelineWindow.startTimeMs : getFirstBookmarkTimeMs());

    const auto dialogCreationFunction = [this, startTimeMs, endTimeMs, filterText]()
    {
        return new QnSearchBookmarksDialog(filterText, startTimeMs, endTimeMs, mainWindow());
    };

    const bool firstTime = m_searchBookmarksDialog.isNull();
    const QnNonModalDialogConstructor<QnSearchBookmarksDialog> creator(m_searchBookmarksDialog, dialogCreationFunction);

    if (!firstTime)
        m_searchBookmarksDialog->setParameters(startTimeMs, endTimeMs, filterText);
}

void QnWorkbenchActionHandler::at_openBusinessLogAction_triggered() {
    QnNonModalDialogConstructor<QnEventLogDialog> dialogConstructor(m_businessEventsLogDialog, mainWindow());

    QnActionParameters parameters = menu()->currentParameters(sender());

    QnBusiness::EventType eventType = parameters.argument(Qn::EventTypeRole, QnBusiness::AnyBusinessEvent);
    QnVirtualCameraResourceList cameras = parameters.resources().filtered<QnVirtualCameraResource>();

    // show diagnostics if Issues action was triggered
    if (eventType != QnBusiness::AnyBusinessEvent || !cameras.isEmpty()) {
        businessEventsLogDialog()->disableUpdateData();
        businessEventsLogDialog()->setEventType(eventType);
        businessEventsLogDialog()->setActionType(QnBusiness::DiagnosticsAction);
        QDate date = QDateTime::currentDateTime().date();
        businessEventsLogDialog()->setDateRange(date, date);
        businessEventsLogDialog()->setCameraList(cameras);
        businessEventsLogDialog()->enableUpdateData();
    }
}

void QnWorkbenchActionHandler::at_openAuditLogAction_triggered() {
    QnNonModalDialogConstructor<QnAuditLogDialog> dialogConstructor(m_auditLogDialog, mainWindow());
    QnActionParameters parameters = menu()->currentParameters(sender());
}

void QnWorkbenchActionHandler::at_cameraListAction_triggered() {
    QnNonModalDialogConstructor<QnCameraListDialog> dialogConstructor(m_cameraListDialog, mainWindow());
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaServerResourcePtr server;
    if (!parameters.resources().isEmpty())
        server = parameters.resource().dynamicCast<QnMediaServerResource>();
    cameraListDialog()->setServer(server);
}

void QnWorkbenchActionHandler::at_thumbnailsSearchAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnResourcePtr resource = parameters.resource();
    if(!resource)
        return;

    QnResourceWidget *widget = parameters.widget();
    if (!widget)
        return;

    bool isSearchLayout = workbench()->currentLayout()->isSearchLayout();

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    QnTimePeriodList periods = parameters.argument<QnTimePeriodList>(Qn::TimePeriodsRole);

    if (period.isEmpty()) {
        if (!isSearchLayout)
            return;

        period = widget->item()->data(Qn::ItemSliderSelectionRole).value<QnTimePeriod>();
        if (period.isEmpty())
            return;

        periods = widget->item()->data(Qn::TimePeriodsRole).value<QnTimePeriodList>();
    }

    /* Adjust for chunks. If they are provided, they MUST intersect with period */
    if(!periods.empty()) {

        QnTimePeriodList localPeriods = periods.intersected(period);

        qint64 startDelta = localPeriods.begin()->startTimeMs - period.startTimeMs;
        if (startDelta > 0) { //user selected period before the first chunk
            period.startTimeMs += startDelta;
            period.durationMs -= startDelta;
        }

        qint64 endDelta = period.endTimeMs() - localPeriods.last().endTimeMs();
        if (endDelta > 0) { // user selected period after the last chunk
            period.durationMs -= endDelta;
        }
    }


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
        QnMessageBox::warning(mainWindow(), tr("Unable to perform preview search."), tr("Selected time period is too short to perform preview search. Please select a longer period."), QMessageBox::Ok);
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

        if(resource->flags() & Qn::utc) {
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

    /* Calculate size of the resulting matrix. */
    qreal desiredItemAspectRatio = qnGlobals->defaultLayoutCellAspectRatio();
    if (widget->hasAspectRatio())
        desiredItemAspectRatio = widget->visualAspectRatio();

    /* Calculate best size for layout cells. */
    qreal desiredCellAspectRatio = desiredItemAspectRatio;

    /* Aspect ratio of the screen free space. */
    QRectF viewportGeometry = display()->boundedViewportGeometry();

    qreal displayAspectRatio = viewportGeometry.isNull()
        ? desiredItemAspectRatio
        : QnGeometry::aspectRatio(viewportGeometry);

    const int matrixWidth = qMax(1, qRound(std::sqrt(displayAspectRatio * itemCount / desiredCellAspectRatio)));

    /* Construct and add a new layout. */
    QnLayoutResourcePtr layout(new QnLayoutResource(qnResTypePool));
    layout->setId(QnUuid::createUuid());
    layout->setName(tr("Preview Search for %1").arg(resource->getName()));
    if(context()->user())
        layout->setParentId(context()->user()->getId());

    qint64 time = period.startTimeMs;
    for(int i = 0; i < itemCount; i++) {
        QnTimePeriod localPeriod (time, step);
        QnTimePeriodList localPeriods = periods.intersected(localPeriod);
        qint64 localTime = time;
        if (!localPeriods.empty())
            localTime = qMax(localTime, localPeriods.begin()->startTimeMs);

        QnLayoutItemData item;
        item.flags = Qn::Pinned;
        item.uuid = QnUuid::createUuid();
        item.combinedGeometry = QRect(i % matrixWidth, i / matrixWidth, 1, 1);
        item.resource.id = resource->getId();
        item.resource.path = resource->getUniqueId();
        item.contrastParams = widget->item()->imageEnhancement();
        item.dewarpingParams = widget->item()->dewarpingParams();
        item.rotation =  widget->item()->rotation();
        item.dataByRole[Qn::ItemPausedRole] = true;
        item.dataByRole[Qn::ItemSliderSelectionRole] = QVariant::fromValue<QnTimePeriod>(localPeriod);
        item.dataByRole[Qn::ItemSliderWindowRole] = QVariant::fromValue<QnTimePeriod>(period);
        item.dataByRole[Qn::ItemTimeRole] = localTime;
        item.dataByRole[Qn::ItemAspectRatioRole] = desiredItemAspectRatio;  // set aspect ratio to make thumbnails load in all cases, see #2619
        item.dataByRole[Qn::TimePeriodsRole] = QVariant::fromValue<QnTimePeriodList>(localPeriods);

        layout->addItem(item);

        time += step;
    }

    layout->setData(Qn::LayoutTimeLabelsRole, true);
    layout->setData(Qn::LayoutSyncStateRole, QVariant::fromValue<QnStreamSynchronizationState>(QnStreamSynchronizationState()));
    layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission));
    layout->setData(Qn::LayoutSearchStateRole, QVariant::fromValue<QnThumbnailsSearchState>(QnThumbnailsSearchState(period, step)));
    layout->setData(Qn::LayoutCellAspectRatioRole, desiredCellAspectRatio);
    layout->setCellAspectRatio(desiredCellAspectRatio);
    layout->setLocalRange(period);

    resourcePool()->addResource(layout);
    menu()->trigger(QnActions::OpenSingleLayoutAction, layout);
}

void QnWorkbenchActionHandler::at_mediaFileSettingsAction_triggered() {
    QnResourcePtr resource = menu()->currentParameters(sender()).resource();
    if (!resource)
        return;

    QnMediaResourcePtr media = resource.dynamicCast<QnMediaResource>();
    if (!media)
        return;

    QScopedPointer<QnMediaFileSettingsDialog> dialog;
    if (resource->hasFlags(Qn::remote))
        dialog.reset(new QnWorkbenchStateDependentDialog<QnMediaFileSettingsDialog>(mainWindow()));
    else
        dialog.reset(new QnMediaFileSettingsDialog(mainWindow()));

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
    menu()->trigger(QnActions::OpenBusinessLogAction,
                    menu()->currentParameters(sender())
                    .withArgument(Qn::EventTypeRole, QnBusiness::AnyCameraEvent));
}

void QnWorkbenchActionHandler::at_cameraBusinessRulesAction_triggered() {
    menu()->trigger(QnActions::OpenBusinessRulesAction,
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

void QnWorkbenchActionHandler::at_serverAddCameraManuallyAction_triggered(){
    QnMediaServerResourceList resources = menu()->currentParameters(sender()).resources().filtered<QnMediaServerResource>();
    if(resources.size() != 1)
        return;

    QnMediaServerResourcePtr server = resources[0];

    QnNonModalDialogConstructor<QnCameraAdditionDialog> dialogConstructor(m_cameraAdditionDialog, mainWindow());

    QnCameraAdditionDialog* dialog = cameraAdditionDialog();

    if (dialog->server() != server) {
        if (dialog->state() == QnCameraAdditionDialog::Searching
                || dialog->state() == QnCameraAdditionDialog::Adding) {

            int result = QnMessageBox::warning(
                        mainWindow(),
                        tr("Process in progress..."),
                        tr("Device addition is already in progress. "
                           "Are you sure you want to cancel current process?"), //TODO: #GDM #Common show current process details
                        QMessageBox::Ok | QMessageBox::Cancel,
                        QMessageBox::Cancel
            );
            if (result != QMessageBox::Ok)
                return;
        }
        dialog->setServer(server);
    }
}

void QnWorkbenchActionHandler::at_serverLogsAction_triggered() {
    QnMediaServerResourcePtr server = menu()->currentParameters(sender()).resource().dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    if (!context()->user())
        return;

    QUrl url = server->getApiUrl();
    url.setScheme(lit("http"));
    url.setPath(lit("/api/showLog"));

    QString login = QnAppServerConnectionFactory::url().userName();
    QString password = QnAppServerConnectionFactory::url().password();
    QUrlQuery urlQuery(url);
    auto nonce = QByteArray::number( qnSyncTime->currentUSecsSinceEpoch(), 16 );
    urlQuery.addQueryItem(
        lit("auth"),
        QLatin1String(createHttpQueryAuthParam(
            login, password, server->realm(), nx_http::Method::GET, nonce)));
    urlQuery.addQueryItem(lit("lines"), QLatin1String("1000"));
    url.setQuery(urlQuery);
    url = QnNetworkProxyFactory::instance()->urlToResource(url, server);
    QDesktopServices::openUrl(url);
}

void QnWorkbenchActionHandler::at_serverIssuesAction_triggered() {
    menu()->trigger(QnActions::OpenBusinessLogAction,
                    QnActionParameters().withArgument(Qn::EventTypeRole, QnBusiness::AnyServerEvent));
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
    QUrl url = QUrl::fromUserInput(resource->getUrl());
    QString host = url.host();
    QnPingDialog *dialog = new QnPingDialog(NULL, Qt::Dialog | Qt::WindowStaysOnTopHint);
    dialog->setHostAddress(host);
    dialog->show();
    dialog->startPings();
#endif

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
        tr("Are you sure you want to permanently delete these %n files?", "", resources.size()),
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
            tr("Are you sure you want to remove these %n items from layout?", "", items.size()),
            QDialogButtonBox::Yes | QDialogButtonBox::No
        );
        if(button != QDialogButtonBox::Yes)
            return;
    }

    QList<QnUuid> orphanedUuids;
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
        foreach(const QnUuid &uuid, orphanedUuids) {
            foreach(QnWorkbenchLayout *layout, layouts) {
                if(QnWorkbenchItem *item = layout->item(uuid)) {
                    qnDeleteLater(item);
                    break;
                }
            }
        }
    }
    if (workbench()->currentLayout()->items().isEmpty())
        workbench()->currentLayout()->setCellAspectRatio(-1.0);
}

bool QnWorkbenchActionHandler::validateResourceName(const QnResourcePtr &resource, const QString &newName) const {
    /* Only users and videowall should be checked. Layouts are checked separately, servers and cameras can have the same name. */
    Qn::ResourceFlags checkedFlags = resource->flags() & (Qn::user | Qn::videowall);
    if (!checkedFlags)
        return true;

    /* Resource cannot have both of these flags at once. */
    Q_ASSERT(checkedFlags == Qn::user || checkedFlags == Qn::videowall);

    foreach (const QnResourcePtr &resource, qnResPool->getResources()) {
        if (!resource->hasFlags(checkedFlags))
            continue;
        if (resource->getName().compare(newName, Qt::CaseInsensitive) != 0)
            continue;

        QString title = checkedFlags == Qn::user
            ? tr("User already exists.")
            : tr("Video Wall already exists");

        QString message = checkedFlags == Qn::user
            ? tr("User with the same name already exists")
            : tr("Video Wall with the same name already exists.");

        QnMessageBox::warning(
            mainWindow(),
            title,
            message
            );
        return false;
    }

    return true;
}


void QnWorkbenchActionHandler::at_renameAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnResourcePtr resource;

    Qn::NodeType nodeType = parameters.argument<Qn::NodeType>(Qn::NodeTypeRole, Qn::ResourceNode);
    switch (nodeType)
    {
        case Qn::ResourceNode:
        case Qn::EdgeNode:
        case Qn::RecorderNode:
            resource = parameters.resource();
            break;
        default:
            break;
    }
    if(!resource)
        return;

    QnVirtualCameraResourcePtr camera;
    if (nodeType == Qn::RecorderNode)
    {
        camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (!camera)
            return;
    }

    QString name = parameters.argument<QString>(Qn::ResourceNameRole).trimmed();
    QString oldName = nodeType == Qn::RecorderNode
            ? camera->getGroupName()
            : resource->getName();

    if (name.isEmpty())
    {
        bool ok = false;
        do
        {
            name = QInputDialog::getText(mainWindow(),
                                         tr("Rename"),
                                         tr("Enter new name for the selected item:"),
                                         QLineEdit::Normal,
                                         oldName,
                                         &ok)
                                         .trimmed();
            if (!ok || name.isEmpty() || name == oldName)
                return;

        }
        while (!validateResourceName(resource, name));
    }

    if(name == oldName)
        return;

    if(QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>())
    {
        context()->instance<QnWorkbenchLayoutsHandler>()->renameLayout(layout, name);
    }
    else if (nodeType == Qn::RecorderNode)
    {
        /* Recorder name should not be validated. */
        QString groupId = camera->getGroupId();

        QnVirtualCameraResourceList modified = qnResPool->getResources().filtered<QnVirtualCameraResource>([groupId](const QnVirtualCameraResourcePtr &camera)
        {
            return camera->getGroupId() == groupId;
        });
        qnResourcesChangesManager->saveCameras(modified, [name](const QnVirtualCameraResourcePtr &camera)
        {
            camera->setUserDefinedGroupName(name);
        });
    }
    else if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
    {
        qnResourcesChangesManager->saveServer(server, [name](const QnMediaServerResourcePtr &server) { server->setName(name); });
    }
    else if (QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        qnResourcesChangesManager->saveCamera(camera, [name](const QnVirtualCameraResourcePtr &camera) { camera->setName(name); });
    }
    else if (QnVideoWallResourcePtr videowall = resource.dynamicCast<QnVideoWallResource>())
    {
        qnResourcesChangesManager->saveVideoWall(videowall, [name](const QnVideoWallResourcePtr &videowall) { videowall->setName(name); } );
    }
    else
    {
        Q_ASSERT_X(false, Q_FUNC_INFO, "Invalid resource type to rename");
    }
}

void QnWorkbenchActionHandler::at_removeFromServerAction_triggered() {
    QnResourceList resources = menu()->currentParameters(sender()).resources();

    /* User cannot delete himself. */
    resources.removeOne(context()->user());

    /* No one can delete Administrator. */
    resources.removeOne(qnResPool->getAdministrator());

    if(resources.isEmpty())
        return;

    auto canAutoDelete = [this](const QnResourcePtr &resource) {
        QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
        if(!layoutResource)
            return false;

        if (qnResPool->isAutoGeneratedLayout(layoutResource))
            return true;

        return snapshotManager()->flags(layoutResource) == Qn::ResourceIsLocal; /* Local, not changed and not being saved. */
    };

    auto deleteResources = [this](const QnResourceList &resources) {

        QnLayoutResourceList localLayouts = resources.filtered<QnLayoutResource>([this](const QnLayoutResourcePtr &layout){
            return snapshotManager()->isLocal(layout);
        });

        QnResourceList remoteResources = resources;

        /* These can be simply deleted from resource pool. */
        for (const QnLayoutResourcePtr &layout: localLayouts) {
            remoteResources.removeOne(layout);
            qnResPool->removeResource(layout);
        }

        qnResourcesChangesManager->deleteResources(remoteResources);
    };


    /* Check if it's OK to delete something without asking. */
    QnResourceList autoDeleting;
    QnResourceList moreResourceToDelete;
    foreach(const QnResourcePtr &resource, resources) {
        if(canAutoDelete(resource))
            autoDeleting << resource;
        else
            moreResourceToDelete << resource;
    }
    if (!autoDeleting.isEmpty())
        deleteResources(autoDeleting);

    foreach (const QnResourcePtr &resource, autoDeleting)
        resources.removeOne(resource);

    if(resources.isEmpty())
        return; /* Nothing to delete. */

    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();

    /* Check that we are deleting online auto-found cameras */
    QnVirtualCameraResourceList onlineAutoDiscoveredCameras = cameras.filtered([](const QnVirtualCameraResourcePtr &camera){
        return camera->getStatus() != Qn::Offline
            && !camera->isManuallyAdded();
    });

    QString question;
    /* First version of the dialog - if all resources are cameras and all of them are auto-discovered. */
    if (resources.size() == onlineAutoDiscoveredCameras.size()) {
        question =
            QnDeviceDependentStrings::getNameFromSet(
                QnCameraDeviceStringSet(
                    tr("These %n devices are auto-discovered. They may be auto-discovered again after removing. Are you sure you want to delete them?",
                        "", onlineAutoDiscoveredCameras.size()),
                    tr("These %n cameras are auto-discovered. They may be auto-discovered again after removing. Are you sure you want to delete them?",
                        "", onlineAutoDiscoveredCameras.size()),
                    tr("These %n I/O modules are auto-discovered. They may be auto-discovered again after removing. Are you sure you want to delete them?",
                        "", onlineAutoDiscoveredCameras.size())
                ),
                onlineAutoDiscoveredCameras
            );
    }
    else
    /* Second version - some cameras are auto-discovered, some not. */
    if (cameras.size() == resources.size() && !onlineAutoDiscoveredCameras.isEmpty()) {
        question =
            QnDeviceDependentStrings::getNameFromSet(
                QnCameraDeviceStringSet(
                    tr("%n of these devices are auto-discovered. They may be auto-discovered again after removing. Are you sure you want to delete them?",
                        "", onlineAutoDiscoveredCameras.size()),
                    tr("%n of these cameras are auto-discovered. They may be auto-discovered again after removing. Are you sure you want to delete them?",
                        "", onlineAutoDiscoveredCameras.size()),
                    tr("%n of these I/O modules are auto-discovered. They may be auto-discovered again after removing. Are you sure you want to delete them?",
                        "", onlineAutoDiscoveredCameras.size())
                ),
                cameras
            );
    }
    else
    /* Third version - no auto-discovered cameras in the list. */
    if (cameras.size() == resources.size()) {
        question =
            QnDeviceDependentStrings::getNameFromSet(
                QnCameraDeviceStringSet(
                    tr("Do you really want to delete the following %n devices?",
                        "", cameras.size()),
                    tr("Do you really want to delete the following %n cameras?",
                        "", cameras.size()),
                    tr("Do you really want to delete the following %n I/O modules?",
                        "", cameras.size())
                ),
                cameras
            );
    }
    else
    /* Forth version - cameras and other items. */
    {
        question =
            tr("Do you really want to delete the following %n items?", "", resources.size());
    }

    if (moreResourceToDelete.isEmpty())
        return;

    int helpId = Qn::Empty_Help;
    for (const QnResourcePtr &resource: resources) {
        if (resource->hasFlags(Qn::live_cam)) {
            helpId = Qn::DeletingCamera_Help;
            break;
        }
        if (resource->hasFlags(Qn::layout)) {
            helpId = Qn::DeletingLayout_Help;
            /* We don't break here because camera could be in the list,
             * and camera has a preference. */
        }
    }

    QDialogButtonBox::StandardButton button = QnResourceListDialog::exec(
        mainWindow(),
        resources,
        helpId,
        tr("Delete Resources"),
        question,
        QDialogButtonBox::Yes | QDialogButtonBox::No
        );
    if (button != QDialogButtonBox::Yes)
        return; /* User does not want it deleted. */

    deleteResources(moreResourceToDelete);
}

void QnWorkbenchActionHandler::at_newUserAction_triggered() {
    QnUserResourcePtr user(new QnUserResource());
    user->setPermissions(Qn::GlobalLiveViewerPermissions);

    QScopedPointer<QnUserSettingsDialog> dialog(new QnUserSettingsDialog(mainWindow()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setUser(user);
    dialog->setElementFlags(QnUserSettingsDialog::CurrentPassword, 0);
    setHelpTopic(dialog.data(), Qn::NewUser_Help);
    do {
        if(!dialog->exec())
            return;
        dialog->submitToResource();
    } while (!validateResourceName(user, user->getName()));

    user->setId(QnUuid::createUuid());
    user->setTypeByName(lit("User"));

    qnResourcesChangesManager->saveUser(user, [](const QnUserResourcePtr &){});
    user->setPassword(QString()); // forget the password now
}

void QnWorkbenchActionHandler::closeApplication(bool force) {
    /* Try close, if force - exit anyway. */
    if (!context()->instance<QnWorkbenchStateManager>()->tryClose(force) && !force)
        return;

    menu()->trigger(QnActions::BeforeExitAction);
    qApp->exit(0);
    applauncher::scheduleProcessKill( QCoreApplication::applicationPid(), PROCESS_TERMINATE_TIMEOUT );
}

void QnWorkbenchActionHandler::at_beforeExitAction_triggered() {
    deleteDialogs();
}


QnAdjustVideoDialog* QnWorkbenchActionHandler::adjustVideoDialog() {
    return m_adjustVideoDialog.data();
}

void QnWorkbenchActionHandler::at_adjustVideoAction_triggered()
{
    QnMediaResourceWidget *widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if(!widget)
        return;

    QnNonModalDialogConstructor<QnAdjustVideoDialog> dialogConstructor(m_adjustVideoDialog, mainWindow());
    adjustVideoDialog()->setWidget(widget);
}

void QnWorkbenchActionHandler::at_userSettingsAction_triggered() {
    QnActionParameters params = menu()->currentParameters(sender());
    QnUserResourcePtr user = params.resource().dynamicCast<QnUserResource>();
    if(!user)
        return;

    Qn::Permissions permissions = accessController()->permissions(user);
    if(!(permissions & Qn::ReadPermission))
        return;

    QScopedPointer<QnUserSettingsDialog> dialog(new QnUserSettingsDialog(mainWindow()));
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

    QnUserSettingsDialog::ElementFlags enabledFlags =
        ((permissions & Qn::WriteAccessRightsPermission) ? QnUserSettingsDialog::Editable | QnUserSettingsDialog::Visible : zero);
    enabledFlags &= flags;

    dialog->setElementFlags(QnUserSettingsDialog::Login, loginFlags);
    dialog->setElementFlags(QnUserSettingsDialog::Password, passwordFlags);
    dialog->setElementFlags(QnUserSettingsDialog::AccessRights, accessRightsFlags);
    dialog->setElementFlags(QnUserSettingsDialog::Email, emailFlags);
    dialog->setElementFlags(QnUserSettingsDialog::Enabled, enabledFlags);


    // TODO #Elric: This is a totally evil hack. Store password hash/salt in user.
    QString currentPassword = QnAppServerConnectionFactory::url().password();
    if(user == context()->user()) {
        dialog->setElementFlags(QnUserSettingsDialog::CurrentPassword, passwordFlags);
        dialog->setCurrentPassword(currentPassword);
    } else {
        dialog->setElementFlags(QnUserSettingsDialog::CurrentPassword, 0);
    }

    dialog->setUser(user);
    if(!dialog->exec() || !dialog->hasChanges())
        return;

    if (!(permissions & Qn::SavePermission))
        return;

    qnResourcesChangesManager->saveUser(user, [&dialog](const QnUserResourcePtr &) {
        dialog->submitToResource();
    });

    //TODO: #GDM SafeMode what to rollback if current password changes cannot be saved?

    QString newPassword = user->getPassword();
    user->setPassword(QString());

    if (user != context()->user() || newPassword.isEmpty() || newPassword == currentPassword)
        return;


    /* Password was changed. Change it in global settings and hope for the best. */
    QUrl url = QnAppServerConnectionFactory::url();
    url.setPassword(newPassword);

    // TODO #elric: This is a totally evil hack. Store password hash/salt in user.
    context()->instance<QnWorkbenchUserWatcher>()->setUserPassword(newPassword);

    QnAppServerConnectionFactory::setUrl(url);

    /* QnAppServerConnectionFactory::url() contains user name in lower case. We'd better use the original name for UI. */
    url.setUserName(user->getName());

    QnConnectionDataList savedConnections = qnSettings->customConnections();
    if (!savedConnections.isEmpty()
        && !savedConnections.first().url.password().isEmpty()
        && qnUrlEqual(savedConnections.first().url, url))
    {
        QnConnectionData current = savedConnections.takeFirst();
        current.url = url;
        savedConnections.prepend(current);
        qnSettings->setCustomConnections(savedConnections);
    }

    QnConnectionData lastUsed = qnSettings->lastUsedConnection();
    if (!lastUsed.url.password().isEmpty() && qnUrlEqual(lastUsed.url, url)) {
        lastUsed.url = url;
        qnSettings->setLastUsedConnection(lastUsed);
    }
}

void QnWorkbenchActionHandler::at_layoutSettingsAction_triggered() {
    QnActionParameters params = menu()->currentParameters(sender());
    openLayoutSettingsDialog(params.resource().dynamicCast<QnLayoutResource>());
}

void QnWorkbenchActionHandler::at_currentLayoutSettingsAction_triggered() {
    openLayoutSettingsDialog(workbench()->currentLayout()->resource());
}

void QnWorkbenchActionHandler::at_setCurrentLayoutItemSpacing0Action_triggered() {
    workbench()->currentLayout()->resource()->setCellSpacing(QSizeF(0.0, 0.0));
    action(QnActions::SetCurrentLayoutItemSpacing0Action)->setChecked(true);
}

void QnWorkbenchActionHandler::at_setCurrentLayoutItemSpacing10Action_triggered() {
    workbench()->currentLayout()->resource()->setCellSpacing(QSizeF(0.1, 0.1));
    action(QnActions::SetCurrentLayoutItemSpacing10Action)->setChecked(true);
}

void QnWorkbenchActionHandler::at_setCurrentLayoutItemSpacing20Action_triggered() {
    workbench()->currentLayout()->resource()->setCellSpacing(QSizeF(0.2, 0.2));
    action(QnActions::SetCurrentLayoutItemSpacing20Action)->setChecked(true);
}

void QnWorkbenchActionHandler::at_setCurrentLayoutItemSpacing30Action_triggered() {
    workbench()->currentLayout()->resource()->setCellSpacing(QSizeF(0.3, 0.3));
    action(QnActions::SetCurrentLayoutItemSpacing30Action)->setChecked(true);
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
    addParams.dewarpingParams.enabled = widget->resource()->getDewarpingParams().enabled;  // zoom items on fisheye cameras must always be dewarped
    addParams.zoomUuid = widget->item()->uuid();
    addParams.frameDistinctionColor = params.argument<QColor>(Qn::ItemFrameDistinctionColorRole);
    addParams.rotation = widget->item()->rotation();

    addToLayout(workbench()->currentLayout()->resource(), widget->resource()->toResourcePtr(), addParams);
}

void QnWorkbenchActionHandler::at_setAsBackgroundAction_triggered() {

    auto checkCondition = [this]() {
        if (!context()->user() || !workbench()->currentLayout() || !workbench()->currentLayout()->resource())
            return false; // action should not be triggered while we are not connected

        if(!accessController()->hasPermissions(workbench()->currentLayout()->resource(), Qn::EditLayoutSettingsPermission))
            return false;

        if (workbench()->currentLayout()->resource()->locked())
            return false;
        return true;
    };

    if (!checkCondition())
        return;

    QnProgressDialog *progressDialog = new QnProgressDialog(mainWindow());
    progressDialog->setWindowTitle(tr("Updating Background..."));
    progressDialog->setLabelText(tr("Image processing may take a few moments. Please be patient."));
    progressDialog->setRange(0, 0);
    progressDialog->setCancelButton(NULL);
    connect(progressDialog, &QnProgressDialog::canceled, progressDialog,     &QObject::deleteLater);

    QnActionParameters parameters = menu()->currentParameters(sender());

    QnAppServerImageCache *cache = new QnAppServerImageCache(this);
    cache->setProperty(uploadingImageARPropertyName, parameters.widget()->aspectRatio());
    connect(cache, &QnAppServerImageCache::fileUploaded,   progressDialog,     &QObject::deleteLater);
    connect(cache, &QnAppServerImageCache::fileUploaded,   cache,              &QObject::deleteLater);
    connect(cache, &QnAppServerImageCache::fileUploaded,   this, [this, checkCondition](const QString &filename, QnAppServerFileCache::OperationResult status) {
        if (!checkCondition())
            return;

        if (status == QnAppServerFileCache::OperationResult::sizeLimitExceeded) {
            QnMessageBox::warning(mainWindow(), tr("Error"), tr("Picture is too big. Maximum size is %1 Mb").arg(QnAppServerFileCache::maximumFileSize() / (1024*1024))
                );
            return;
        }

        if (status != QnAppServerFileCache::OperationResult::ok) {
            QnMessageBox::warning(mainWindow(), tr("Error"), tr("Error while uploading picture."));
            return;
        }

        setCurrentLayoutBackground(filename);

    });

    cache->storeImage(parameters.resource()->getUrl());
    progressDialog->exec();
}

void QnWorkbenchActionHandler::setCurrentLayoutBackground(const QString &filename) {
    QnWorkbenchLayout* wlayout = workbench()->currentLayout();
    QnLayoutResourcePtr layout = wlayout->resource();

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

void QnWorkbenchActionHandler::at_panicWatcher_panicModeChanged() {
    action(QnActions::TogglePanicModeAction)->setChecked(context()->instance<QnWorkbenchPanicWatcher>()->isPanicMode());
    //if (!action(QnActions::TogglePanicModeAction)->isChecked()) {

    bool enabled =
        context()->instance<QnWorkbenchScheduleWatcher>()->isScheduleEnabled() &&
        (accessController()->globalPermissions() & Qn::GlobalPanicPermission);
    action(QnActions::TogglePanicModeAction)->setEnabled(enabled);

    //}
}

void QnWorkbenchActionHandler::at_scheduleWatcher_scheduleEnabledChanged() {
    // TODO: #Elric totally evil copypasta and hacky workaround.
    bool enabled =
        context()->instance<QnWorkbenchScheduleWatcher>()->isScheduleEnabled() &&
        (accessController()->globalPermissions() & Qn::GlobalPanicPermission);

    action(QnActions::TogglePanicModeAction)->setEnabled(enabled);
    if (!enabled)
        action(QnActions::TogglePanicModeAction)->setChecked(false);
}

void QnWorkbenchActionHandler::at_togglePanicModeAction_toggled(bool checked) {
    QnMediaServerResourceList resources = resourcePool()->getResources<QnMediaServerResource>();

    foreach(QnMediaServerResourcePtr resource, resources)
    {
        bool isPanicMode = resource->getPanicMode() != Qn::PM_None;
        if(isPanicMode != checked) {
            Qn::PanicMode val = Qn::PM_None;
            if (checked)
                val = Qn::PM_User;
            resource->setPanicMode(val);
            propertyDictionary->saveParamsAsync(resource->getId());
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
        action(QnActions::ToggleTourModeAction)->setChecked(false);
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
        action(QnActions::ToggleTourModeAction)->setChecked(false);

    if (role == Qn::CentralRole && adjustVideoDialog() && adjustVideoDialog()->isVisible())
    {
        QnWorkbenchItem *item = context()->workbench()->item(Qn::CentralRole);
        QnMediaResourceWidget* widget = dynamic_cast<QnMediaResourceWidget*> (context()->display()->widget(item));
        if (widget)
            adjustVideoDialog()->setWidget(widget);
    }
}

void QnWorkbenchActionHandler::at_whatsThisAction_triggered() {
    if (QWhatsThis::inWhatsThisMode())
        QWhatsThis::leaveWhatsThisMode();
    else
        QWhatsThis::enterWhatsThisMode();
}

void QnWorkbenchActionHandler::at_escapeHotkeyAction_triggered() {
    if (QWhatsThis::inWhatsThisMode()) {
        QWhatsThis::leaveWhatsThisMode();
        return;
    }

    if (action(QnActions::ToggleTourModeAction)->isChecked())
        menu()->trigger(QnActions::ToggleTourModeAction);
}

void QnWorkbenchActionHandler::at_messageBoxAction_triggered() {
    QString title = menu()->currentParameters(sender()).argument<QString>(Qn::TitleRole);
    QString text = menu()->currentParameters(sender()).argument<QString>(Qn::TextRole);
    if (text.isEmpty())
        text = title;

    QnMessageBox::information(mainWindow(), title, text);
}

void QnWorkbenchActionHandler::at_browseUrlAction_triggered() {
    QString url = menu()->currentParameters(sender()).argument<QString>(Qn::UrlRole);
    if (url.isEmpty())
        return;

    QDesktopServices::openUrl(QUrl::fromUserInput(url));
}

void QnWorkbenchActionHandler::at_versionMismatchMessageAction_triggered() {
    if (qnCommon->isReadOnly())
        return;

    QnWorkbenchVersionMismatchWatcher *watcher = context()->instance<QnWorkbenchVersionMismatchWatcher>();
    if(!watcher->hasMismatches())
        return;

    QnSoftwareVersion latestVersion = watcher->latestVersion();
    QnSoftwareVersion latestMsVersion = watcher->latestVersion(Qn::ServerComponent);

    // if some component is newer than the newest mediaserver, focus on its version
    if (QnWorkbenchVersionMismatchWatcher::versionMismatches(latestVersion, latestMsVersion))
        latestMsVersion = latestVersion;

    QStringList messageParts;
    messageParts << tr("Some components of the system are not updated");
    messageParts << QString();

    foreach(const QnAppInfoMismatchData &data, watcher->mismatchData()) {
        QString component;
        switch(data.component) {
        case Qn::ClientComponent:
            component = tr("Client v%1").arg(data.version.toString());
            break;
        case Qn::ServerComponent: {
            QnMediaServerResourcePtr resource = data.resource.dynamicCast<QnMediaServerResource>();
            if(resource) {
                component = tr("Server v%1 at %2").arg(data.version.toString()).arg(QUrl(resource->getUrl()).host());
            } else {
                component = tr("Server v%1").arg(data.version.toString());
            }
        }
        default:
            break;
        }

        bool updateRequested = (data.component == Qn::ServerComponent) &&
            QnWorkbenchVersionMismatchWatcher::versionMismatches(data.version, latestMsVersion, true);

        if (updateRequested)
            component = setWarningStyleHtml(component);

        messageParts << component;
    }

    messageParts << QString();
    messageParts << tr("Please update all components to the latest version %1.").arg(latestMsVersion.toString());

    QString message = messageParts.join(lit("<br/>"));

    QScopedPointer<QnWorkbenchStateDependentDialog<QnMessageBox> > messageBox(
        new QnWorkbenchStateDependentDialog<QnMessageBox>(mainWindow()));
    messageBox->setIcon(QMessageBox::Warning);
    messageBox->setWindowTitle(tr("Version Mismatch"));
    messageBox->setText(message);
    messageBox->setTextFormat(Qt::RichText);
    messageBox->setStandardButtons(QMessageBox::Cancel);
    setHelpTopic(messageBox.data(), Qn::Upgrade_Help);

    QPushButton *updateButton = messageBox->addButton(tr("Update..."), QMessageBox::HelpRole);
    connect(updateButton, &QPushButton::clicked, this, [this] {
        menu()->trigger(QnActions::SystemUpdateAction);
    }, Qt::QueuedConnection);

    messageBox->exec();
}

void QnWorkbenchActionHandler::at_betaVersionMessageAction_triggered() {
    QnMessageBox::warning(mainWindow(),
                         tr("Beta version %1").arg(QnAppInfo::applicationVersion()),
                         tr("This is a beta version of %1.")
                         .arg(qApp->applicationDisplayName()));
}

void QnWorkbenchActionHandler::checkIfStatisticsReportAllowed() {

    const QnMediaServerResourceList servers = qnResPool->getResources<QnMediaServerResource>();

    /* Check if we are not connected yet. */
    if (servers.isEmpty())
        return;

    /* Check if user has already made a decision. */
    if (qnGlobalSettings->isStatisticsAllowedDefined())
        return;

    /* User cannot disable statistics collecting, so don't make him sorrow. */
    if (qnCommon->isReadOnly())
        return;

    /* Suppress notification if no server has internet access. */
    bool atLeastOneServerHasInternetAccess = boost::algorithm::any_of(servers, [](const QnMediaServerResourcePtr &server) {
        return (server->getServerFlags() & Qn::SF_HasPublicIP);
    });
    if (!atLeastOneServerHasInternetAccess)
        return;

    QnMessageBox::information(
        mainWindow(),
        tr("Anonymous Usage Statistics"),
        tr("System sends anonymous usage and crash statistics to the software development team to help us improve your user experience.\n"
           "If you would like to disable this feature you can do so in the System Settings dialog.")
           );

    qnGlobalSettings->setStatisticsAllowed(true);
    qnGlobalSettings->synchronizeNow();
}


void QnWorkbenchActionHandler::at_queueAppRestartAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnSoftwareVersion version = parameters.hasArgument(Qn::SoftwareVersionRole)
            ? parameters.argument<QnSoftwareVersion>(Qn::SoftwareVersionRole)
            : QnSoftwareVersion();
    QUrl url = parameters.hasArgument(Qn::UrlRole)
            ? parameters.argument<QUrl>(Qn::UrlRole)
            : context()->user()
              ? QnAppServerConnectionFactory::url()
              : QUrl();
    QByteArray auth = url.toEncoded();

    bool isInstalled = false;
    bool success = applauncher::isVersionInstalled(version, &isInstalled) == applauncher::api::ResultType::ok;
    if (success && isInstalled)
        success = applauncher::restartClient(version, auth) == applauncher::api::ResultType::ok;

    if (!success) {
        QMessageBox::critical(
                    mainWindow(),
                    tr("Launcher process not found."),
                    tr("Cannot restart the client.") + L'\n'
                  + tr("Please close the application and start it again using the shortcut in the start menu.")
                    );
        return;
    }
    menu()->trigger(QnActions::DelayedForcedExitAction);
}

void QnWorkbenchActionHandler::at_selectTimeServerAction_triggered() {
    QnNonModalDialogConstructor<QnSystemAdministrationDialog> dialogConstructor(m_systemAdministrationDialog, mainWindow());
    systemAdministrationDialog()->setCurrentPage(QnSystemAdministrationDialog::TimeServerSelection);
}

void QnWorkbenchActionHandler::deleteDialogs() {
    if (businessRulesDialog())
        delete businessRulesDialog();

    if (businessEventsLogDialog())
        delete businessEventsLogDialog();

    if (cameraAdditionDialog())
        delete cameraAdditionDialog();

    if (adjustVideoDialog())
        delete adjustVideoDialog();

    if (cameraListDialog())
        delete cameraListDialog();

    if (systemAdministrationDialog())
        delete systemAdministrationDialog();
}
