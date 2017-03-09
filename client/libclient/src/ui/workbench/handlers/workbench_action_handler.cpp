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

#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client/desktop_client_message_processor.h>
#include <client/client_show_once_settings.h>

#include <common/common_module.h>

#include <core/resource_access/resource_access_filter.h>

#include <core/resource/resource.h>
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
#include <core/resource_management/resource_runtime_data.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource/file_processor.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/videowall_item_index.h>

#include <nx_ec/dummy_handler.h>

#include <nx/client/messages/resources_messages.h>
#include <nx/client/messages/videowall_messages.h>

#include <nx/network/http/httptypes.h>
#include <nx/network/socket_global.h>
#include <nx/network/cloud/address_resolver.h>

#include <nx/streaming/archive_stream_reader.h>

#include <network/cloud_url_validator.h>

#include <plugins/resource/avi/avi_resource.h>
#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <platform/environment.h>

#include <recording/time_period_list.h>

#include <redass/redass_controller.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_parameter_types.h>
#include <ui/actions/action_target_provider.h>

#include <ui/dialogs/about_dialog.h>
#include <ui/dialogs/connection_testing_dialog.h>
#include <ui/dialogs/local_settings_dialog.h>
#include <ui/dialogs/camera_addition_dialog.h>
#include <ui/dialogs/common/progress_dialog.h>
#include <ui/dialogs/business_rules_dialog.h>
#include <ui/dialogs/failover_priority_dialog.h>
#include <ui/dialogs/backup_cameras_dialog.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_dialog.h>
#include <ui/dialogs/camera_diagnostics_dialog.h>
#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/dialogs/media_file_settings_dialog.h>
#include <ui/dialogs/ping_dialog.h>
#include <ui/dialogs/system_administration_dialog.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/style/globals.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>

#include <ui/widgets/views/resource_list_view.h>

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
#include <ui/workbench/workbench_welcome_screen.h>

#include <ui/workbench/handlers/workbench_layouts_handler.h>    //TODO: #GDM dependencies

#include <ui/workbench/watchers/workbench_user_watcher.h>
#include <ui/workbench/watchers/workbench_panic_watcher.h>
#include <ui/workbench/watchers/workbench_schedule_watcher.h>
#include <ui/workbench/watchers/workbench_update_watcher.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>
#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>
#include <ui/workbench/watchers/workbench_bookmarks_watcher.h>

#include <utils/app_server_image_cache.h>

#include <utils/applauncher_utils.h>
#include <utils/local_file_cache.h>
#include <utils/common/delete_later.h>
#include <utils/common/mime_data.h>
#include <utils/common/event_processors.h>
#include <nx/utils/string.h>
#include <utils/common/time.h>
#include <utils/common/synctime.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/url.h>
#include <utils/email/email.h>
#include <utils/math/math.h>
#include <nx/utils/std/cpp14.h>
#include <utils/screen_manager.h>
#include <vms_gateway_embeddable.h>
#include <utils/unity_launcher_workaround.h>
#include <utils/connection_diagnostics_helper.h>

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
#include <core/resource/fake_media_server.h>

namespace {

/* Beta version message. */
static const QString kBetaVersionShowOnceKey(lit("BetaVersion"));

/* Asking for update all outdated servers to the last version. */
static const QString kVersionMismatchShowOnceKey(lit("VersionMismatch"));

const char* uploadingImageARPropertyName = "_qn_uploadingImageARPropertyName";

} // namespace

//!time that is given to process to exit. After that, applauncher (if present) will try to terminate it
static const quint32 PROCESS_TERMINATE_TIMEOUT = 15000;

using nx::client::desktop::utils::UnityLauncherWorkaround;

// -------------------------------------------------------------------------- //
// QnWorkbenchActionHandler
// -------------------------------------------------------------------------- //
QnWorkbenchActionHandler::QnWorkbenchActionHandler(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_delayedDropGuard(false),
    m_tourTimer(new QTimer(this))
{
    connect(m_tourTimer, SIGNAL(timeout()), this, SLOT(at_tourTimer_timeout()));
    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnWorkbenchActionHandler::at_context_userChanged);

    connect(workbench(), SIGNAL(itemChanged(Qn::ItemRole)), this, SLOT(at_workbench_itemChanged(Qn::ItemRole)));
    connect(workbench(), SIGNAL(cellSpacingChanged()), this, SLOT(at_workbench_cellSpacingChanged()));
    connect(workbench(), SIGNAL(currentLayoutChanged()), this, SLOT(at_workbench_currentLayoutChanged()));

    connect(action(QnActions::AboutAction), SIGNAL(triggered()), this, SLOT(at_aboutAction_triggered()));
    connect(action(QnActions::OpenFileAction), SIGNAL(triggered()), this, SLOT(at_openFileAction_triggered()));
    connect(action(QnActions::OpenFolderAction), SIGNAL(triggered()), this, SLOT(at_openFolderAction_triggered()));

    // local settings
    connect(action(QnActions::PreferencesGeneralTabAction), &QAction::triggered, this,
        [this] { openLocalSettingsDialog(QnLocalSettingsDialog::GeneralPage); },
        Qt::QueuedConnection);
    connect(action(QnActions::PreferencesNotificationTabAction), &QAction::triggered, this,
        [this] { openLocalSettingsDialog(QnLocalSettingsDialog::NotificationsPage); },
        Qt::QueuedConnection);

    // system administration
    connect(action(QnActions::SystemAdministrationAction), &QAction::triggered, this,
        [this] { openSystemAdministrationDialog(QnSystemAdministrationDialog::GeneralPage); });
    connect(action(QnActions::PreferencesLicensesTabAction), &QAction::triggered, this,
        [this] { openSystemAdministrationDialog(QnSystemAdministrationDialog::LicensesPage); });
    connect(action(QnActions::PreferencesSmtpTabAction), &QAction::triggered, this,
        [this] { openSystemAdministrationDialog(QnSystemAdministrationDialog::SmtpPage); });
    connect(action(QnActions::PreferencesCloudTabAction), &QAction::triggered, this,
        [this]{ openSystemAdministrationDialog(QnSystemAdministrationDialog::CloudManagement); });
    connect(action(QnActions::SystemUpdateAction), &QAction::triggered, this,
        [this] { openSystemAdministrationDialog(QnSystemAdministrationDialog::UpdatesPage); });
    connect(action(QnActions::UserManagementAction), &QAction::triggered, this,
        [this] { openSystemAdministrationDialog(QnSystemAdministrationDialog::UserManagement); });

    connect(action(QnActions::BusinessEventsAction), SIGNAL(triggered()), this, SLOT(at_businessEventsAction_triggered()));
    connect(action(QnActions::OpenBusinessRulesAction), SIGNAL(triggered()), this, SLOT(at_openBusinessRulesAction_triggered()));
    connect(action(QnActions::OpenFailoverPriorityAction), &QAction::triggered, this, &QnWorkbenchActionHandler::openFailoverPriorityDialog);
    connect(action(QnActions::OpenBackupCamerasAction), &QAction::triggered, this, &QnWorkbenchActionHandler::openBackupCamerasDialog);
    connect(action(QnActions::OpenBookmarksSearchAction), SIGNAL(triggered()), this, SLOT(at_openBookmarksSearchAction_triggered()));
    connect(action(QnActions::OpenBusinessLogAction), SIGNAL(triggered()), this, SLOT(at_openBusinessLogAction_triggered()));
    connect(action(QnActions::OpenAuditLogAction), SIGNAL(triggered()), this, SLOT(at_openAuditLogAction_triggered()));
    connect(action(QnActions::CameraListAction), SIGNAL(triggered()), this, SLOT(at_cameraListAction_triggered()));
    connect(action(QnActions::CameraListByServerAction), SIGNAL(triggered()), this, SLOT(at_cameraListAction_triggered()));

    connect(action(QnActions::WebClientAction), &QAction::triggered, this,
        &QnWorkbenchActionHandler::at_webClientAction_triggered);
    connect(action(QnActions::WebAdminAction), &QAction::triggered, this,
        &QnWorkbenchActionHandler::at_webAdminAction_triggered);

    connect(action(QnActions::NextLayoutAction), SIGNAL(triggered()), this, SLOT(at_nextLayoutAction_triggered()));
    connect(action(QnActions::PreviousLayoutAction), SIGNAL(triggered()), this, SLOT(at_previousLayoutAction_triggered()));
    connect(action(QnActions::OpenInLayoutAction), SIGNAL(triggered()), this, SLOT(at_openInLayoutAction_triggered()));
    connect(action(QnActions::OpenInCurrentLayoutAction), SIGNAL(triggered()), this, SLOT(at_openInCurrentLayoutAction_triggered()));
    connect(action(QnActions::OpenInNewLayoutAction), SIGNAL(triggered()), this, SLOT(at_openInNewLayoutAction_triggered()));
    connect(action(QnActions::OpenInNewWindowAction), SIGNAL(triggered()), this, SLOT(at_openInNewWindowAction_triggered()));
    connect(action(QnActions::OpenSingleLayoutAction), SIGNAL(triggered()), this, SLOT(at_openLayoutsAction_triggered()));
    connect(action(QnActions::OpenMultipleLayoutsAction), SIGNAL(triggered()), this, SLOT(at_openLayoutsAction_triggered()));
    connect(action(QnActions::OpenAnyNumberOfLayoutsAction), SIGNAL(triggered()), this, SLOT(at_openLayoutsAction_triggered()));
    connect(action(QnActions::OpenLayoutsInNewWindowAction), SIGNAL(triggered()), this, SLOT(at_openLayoutsInNewWindowAction_triggered()));
    connect(action(QnActions::OpenCurrentLayoutInNewWindowAction), SIGNAL(triggered()), this, SLOT(at_openCurrentLayoutInNewWindowAction_triggered()));
    connect(action(QnActions::OpenNewWindowAction), SIGNAL(triggered()), this, SLOT(at_openNewWindowAction_triggered()));

    connect(action(QnActions::MediaFileSettingsAction), &QAction::triggered, this, &QnWorkbenchActionHandler::at_mediaFileSettingsAction_triggered);
    connect(action(QnActions::CameraIssuesAction), SIGNAL(triggered()), this, SLOT(at_cameraIssuesAction_triggered()));
    connect(action(QnActions::CameraBusinessRulesAction), SIGNAL(triggered()), this, SLOT(at_cameraBusinessRulesAction_triggered()));
    connect(action(QnActions::CameraDiagnosticsAction), SIGNAL(triggered()), this, SLOT(at_cameraDiagnosticsAction_triggered()));
    connect(action(QnActions::ServerAddCameraManuallyAction), SIGNAL(triggered()), this, SLOT(at_serverAddCameraManuallyAction_triggered()));
    connect(action(QnActions::PingAction), SIGNAL(triggered()), this, SLOT(at_pingAction_triggered()));
    connect(action(QnActions::ServerLogsAction), SIGNAL(triggered()), this, SLOT(at_serverLogsAction_triggered()));
    connect(action(QnActions::ServerIssuesAction), SIGNAL(triggered()), this, SLOT(at_serverIssuesAction_triggered()));
    connect(action(QnActions::OpenInFolderAction), SIGNAL(triggered()), this, SLOT(at_openInFolderAction_triggered()));
    connect(action(QnActions::DeleteFromDiskAction), SIGNAL(triggered()), this, SLOT(at_deleteFromDiskAction_triggered()));
    connect(action(QnActions::RemoveFromServerAction), SIGNAL(triggered()), this, SLOT(at_removeFromServerAction_triggered()));
    connect(action(QnActions::RenameResourceAction), SIGNAL(triggered()), this, SLOT(at_renameAction_triggered()));
    connect(action(QnActions::DropResourcesAction), SIGNAL(triggered()), this, SLOT(at_dropResourcesAction_triggered()));
    connect(action(QnActions::DelayedDropResourcesAction), SIGNAL(triggered()), this, SLOT(at_delayedDropResourcesAction_triggered()));
    connect(action(QnActions::InstantDropResourcesAction), SIGNAL(triggered()), this, SLOT(at_instantDropResourcesAction_triggered()));
    connect(action(QnActions::DropResourcesIntoNewLayoutAction), SIGNAL(triggered()), this, SLOT(at_dropResourcesIntoNewLayoutAction_triggered()));
    connect(action(QnActions::MoveCameraAction), SIGNAL(triggered()), this, SLOT(at_moveCameraAction_triggered()));
    connect(action(QnActions::AdjustVideoAction), SIGNAL(triggered()), this, SLOT(at_adjustVideoAction_triggered()));
    connect(action(QnActions::ExitAction), &QAction::triggered, this, &QnWorkbenchActionHandler::closeApplication);
    connect(action(QnActions::ThumbnailsSearchAction), SIGNAL(triggered()), this, SLOT(at_thumbnailsSearchAction_triggered()));
    connect(action(QnActions::CreateZoomWindowAction), SIGNAL(triggered()), this, SLOT(at_createZoomWindowAction_triggered()));

    connect(action(QnActions::SetCurrentLayoutItemSpacingNoneAction), &QAction::triggered, this,
        [this]
        {
            setCurrentLayoutCellSpacing(Qn::CellSpacing::None);
        });

    connect(action(QnActions::SetCurrentLayoutItemSpacingSmallAction), &QAction::triggered, this,
        [this]
        {
            setCurrentLayoutCellSpacing(Qn::CellSpacing::Small);
        });

    connect(action(QnActions::SetCurrentLayoutItemSpacingMediumAction), &QAction::triggered, this,
        [this]
        {
            setCurrentLayoutCellSpacing(Qn::CellSpacing::Medium);
        });

    connect(action(QnActions::SetCurrentLayoutItemSpacingLargeAction), &QAction::triggered, this,
        [this]
        {
            setCurrentLayoutCellSpacing(Qn::CellSpacing::Large);
        });

    connect(action(QnActions::Rotate0Action), &QAction::triggered, this, [this] { rotateItems(0); });
    connect(action(QnActions::Rotate90Action), &QAction::triggered, this, [this] { rotateItems(90); });
    connect(action(QnActions::Rotate180Action), &QAction::triggered, this, [this] { rotateItems(180); });
    connect(action(QnActions::Rotate270Action), &QAction::triggered, this, [this] { rotateItems(270); });
    connect(action(QnActions::RadassAutoAction), &QAction::triggered, this, [this] { setResolutionMode(Qn::AutoResolution); });
    connect(action(QnActions::RadassLowAction), &QAction::triggered, this, [this] { setResolutionMode(Qn::LowResolution); });
    connect(action(QnActions::RadassHighAction), &QAction::triggered, this, [this] { setResolutionMode(Qn::HighResolution); });
    connect(action(QnActions::SetAsBackgroundAction), SIGNAL(triggered()), this, SLOT(at_setAsBackgroundAction_triggered()));
    connect(action(QnActions::WhatsThisAction), SIGNAL(triggered()), this, SLOT(at_whatsThisAction_triggered()));
    connect(action(QnActions::EscapeHotkeyAction), SIGNAL(triggered()), this, SLOT(at_escapeHotkeyAction_triggered()));
    connect(action(QnActions::BrowseUrlAction), SIGNAL(triggered()), this, SLOT(at_browseUrlAction_triggered()));
    connect(action(QnActions::VersionMismatchMessageAction), &QAction::triggered, this, &QnWorkbenchActionHandler::at_versionMismatchMessageAction_triggered);
    connect(action(QnActions::BetaVersionMessageAction), SIGNAL(triggered()), this, SLOT(at_betaVersionMessageAction_triggered()));
    connect(action(QnActions::AllowStatisticsReportMessageAction), &QAction::triggered, this, [this] { checkIfStatisticsReportAllowed(); });

    connect(action(QnActions::QueueAppRestartAction), SIGNAL(triggered()), this, SLOT(at_queueAppRestartAction_triggered()));
    connect(action(QnActions::SelectTimeServerAction), SIGNAL(triggered()), this, SLOT(at_selectTimeServerAction_triggered()));

    connect(action(QnActions::TogglePanicModeAction), SIGNAL(toggled(bool)), this, SLOT(at_togglePanicModeAction_toggled(bool)));
    connect(action(QnActions::ToggleTourModeAction), SIGNAL(toggled(bool)), this, SLOT(at_toggleTourAction_toggled(bool)));
    //connect(context()->instance<QnWorkbenchPanicWatcher>(),     SIGNAL(panicModeChanged()), this, SLOT(at_panicWatcher_panicModeChanged()));
    connect(context()->instance<QnWorkbenchScheduleWatcher>(), SIGNAL(scheduleEnabledChanged()), this, SLOT(at_scheduleWatcher_scheduleEnabledChanged()));

    /* Connect through lambda to handle forced parameter. */
    connect(action(QnActions::DelayedForcedExitAction), &QAction::triggered, this, [this] {  closeApplication(true);    }, Qt::QueuedConnection);

    connect(action(QnActions::BeforeExitAction), &QAction::triggered, this, &QnWorkbenchActionHandler::at_beforeExitAction_triggered);

    /* Run handlers that update state. */
    //at_panicWatcher_panicModeChanged();
    at_scheduleWatcher_scheduleEnabledChanged();
}

QnWorkbenchActionHandler::~QnWorkbenchActionHandler()
{
    deleteDialogs();
}

void QnWorkbenchActionHandler::addToLayout(const QnLayoutResourcePtr &layout, const QnResourcePtr &resource, const AddToLayoutParams &params) const
{
    if (!layout)
        return;

    if (qnSettings->lightMode() & Qn::LightModeSingleItem) {
        while (!layout->getItems().isEmpty())
            layout->removeItem(*(layout->getItems().begin()));
    }

    int maxItems = (qnSettings->lightMode() & Qn::LightModeSingleItem)
        ? 1
        : qnSettings->maxSceneVideoItems();

    if (layout->getItems().size() >= maxItems)
        return;


    if (!menu()->canTrigger(QnActions::OpenInLayoutAction, QnActionParameters(resource)
        .withArgument(Qn::LayoutResourceRole, layout)))
    {
        return;
    }

    QnLayoutItemData data;
    data.resource.id = resource->getId();
    data.resource.uniqueId = resource->getUniqueId();
    data.uuid = QnUuid::createUuid();
    data.flags = Qn::PendingGeometryAdjustment;
    data.zoomRect = params.zoomWindow;
    data.zoomTargetUuid = params.zoomUuid;

    if (!qFuzzyIsNull(params.rotation))
    {
        data.rotation = params.rotation;
    }
    else
    {
        QString forcedRotation = resource->getProperty(QnMediaResource::rotationKey());
        if (!forcedRotation.isEmpty())
            data.rotation = forcedRotation.toInt();
    }
    data.contrastParams = params.contrastParams;
    data.dewarpingParams = params.dewarpingParams;

    qnResourceRuntimeDataManager->setLayoutItemData(data.uuid, Qn::ItemTimeRole, params.time);
    if (params.frameDistinctionColor.isValid())
        qnResourceRuntimeDataManager->setLayoutItemData(data.uuid, Qn::ItemFrameDistinctionColorRole, params.frameDistinctionColor);

    if (params.usePosition)
        data.combinedGeometry = QRectF(params.position, params.position); /* Desired position is encoded into a valid rect. */
    else
        data.combinedGeometry = QRectF(QPointF(0.5, 0.5), QPointF(-0.5, -0.5)); /* The fact that any position is OK is encoded into an invalid rect. */
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

void QnWorkbenchActionHandler::openResourcesInNewWindow(const QnResourceList &resources) {
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

    if (context()->user())
    {
        arguments << lit("--auth");
        arguments << QnStartupParameters::createAuthenticationString(
            QnAppServerConnectionFactory::url());
    }

    if (mainWindow())
    {
        int screen = context()->instance<QnScreenManager>()->nextFreeScreen();
        arguments << QnStartupParameters::kScreenKey << QString::number(screen);
    }

    if (qnRuntime->isDevMode())
        arguments << lit("--dev-mode-key=razrazraz");

#ifdef Q_OS_MACX
    mac_startDetached(qApp->applicationFilePath(), arguments);
#else
    UnityLauncherWorkaround::startDetached(qApp->applicationFilePath(), arguments);
#endif
}

void QnWorkbenchActionHandler::rotateItems(int degrees) {
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();
    if (!widgets.empty()) {
        foreach(const QnResourceWidget *widget, widgets)
            widget->item()->setRotation(degrees);
    }
}

void QnWorkbenchActionHandler::setResolutionMode(Qn::ResolutionMode resolutionMode) {
    if (qnRedAssController)
        qnRedAssController->setMode(resolutionMode);
}

void QnWorkbenchActionHandler::setCurrentLayoutCellSpacing(Qn::CellSpacing spacing)
{
    //TODO: #GDM #3.1 move out these actions to separate CurrentLayoutHandler
    // There at_workbench_cellSpacingChanged will also use this method
    auto actionId = [spacing]
        {
            switch (spacing)
            {
                case Qn::CellSpacing::None:
                    return QnActions::SetCurrentLayoutItemSpacingNoneAction;
                case Qn::CellSpacing::Small:
                    return QnActions::SetCurrentLayoutItemSpacingSmallAction;
                case Qn::CellSpacing::Medium:
                    return QnActions::SetCurrentLayoutItemSpacingMediumAction;
                case Qn::CellSpacing::Large:
                    return QnActions::SetCurrentLayoutItemSpacingLargeAction;
            }
            NX_ASSERT(false);
            return QnActions::SetCurrentLayoutItemSpacingSmallAction;
        };

    if (auto layout = workbench()->currentLayout()->resource())
    {
        layout->setCellSpacing(QnWorkbenchLayout::cellSpacingValue(spacing));
        action(actionId())->setChecked(true);
    }
}

QnBusinessRulesDialog *QnWorkbenchActionHandler::businessRulesDialog() const
{
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

    if (!context()->user())
        return;

    if (!context()->workbench()->currentLayout()->resource())
        return;


    QN_SCOPED_VALUE_ROLLBACK(&m_delayedDropGuard, true);

    foreach(const QnMimeData &data, m_delayedDrops) {
        QMimeData mimeData;
        data.toMimeData(&mimeData);

        QnResourceList resources = QnWorkbenchResource::deserializeResources(&mimeData);
        QnLayoutResourceList layouts = resources.filtered<QnLayoutResource>();
        if (!layouts.isEmpty()) {
            workbench()->clear();
            menu()->trigger(QnActions::OpenAnyNumberOfLayoutsAction, layouts);
        }
        else {
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
        if (!layouts.isEmpty()) {
            workbench()->clear();
            menu()->trigger(QnActions::OpenAnyNumberOfLayoutsAction, layouts);
        }
        else {
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
    m_serverRequests.clear();

    if (!user)
        return;

    /* We should not change state when using "Open in New Window". Otherwise workbench will be
     * cleared here even if no state is saved. */
    if (m_delayedDrops.isEmpty() && qnRuntime->isDesktopMode())
        context()->instance<QnWorkbenchStateManager>()->restoreState();

    /* Sometimes we get here when 'New Layout' has already been added. But all user's layouts must be created AFTER this method.
    * Otherwise the user will see uncreated layouts in layout selection menu.
    * As temporary workaround we can just remove that layouts. */
    // TODO: #dklychkov Do not create new empty layout before this method end. See: at_openNewTabAction_triggered()
    if (user && !qnRuntime->isActiveXMode())
    {
        for (const QnLayoutResourcePtr &layout : qnResPool->getResourcesWithParentId(user->getId()).filtered<QnLayoutResource>())
        {
            if (layout->hasFlags(Qn::local) && !layout->isFile())
                qnResPool->removeResource(layout);
        }
    }

    if (workbench()->layouts().empty())
        menu()->trigger(QnActions::OpenNewTabAction);

    submitDelayedDrops();
}

void QnWorkbenchActionHandler::at_workbench_cellSpacingChanged()
{
    qreal value = workbench()->currentLayout()->cellSpacing();

    if (qFuzzyIsNull(value))
        action(QnActions::SetCurrentLayoutItemSpacingNoneAction)->setChecked(true);
    else if (qFuzzyCompare(QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Medium), value))
        action(QnActions::SetCurrentLayoutItemSpacingMediumAction)->setChecked(true);
    else if (qFuzzyCompare(QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Large), value))
        action(QnActions::SetCurrentLayoutItemSpacingLargeAction)->setChecked(true);
    else
        action(QnActions::SetCurrentLayoutItemSpacingSmallAction)->setChecked(true); //default value
}

void QnWorkbenchActionHandler::at_workbench_currentLayoutChanged() {
    action(QnActions::RadassAutoAction)->setChecked(true);
    if (qnRedAssController)
        qnRedAssController->setMode(Qn::AutoResolution);
}

void QnWorkbenchActionHandler::at_nextLayoutAction_triggered() {
    workbench()->setCurrentLayoutIndex((workbench()->currentLayoutIndex() + 1) % workbench()->layouts().size());
}

void QnWorkbenchActionHandler::at_previousLayoutAction_triggered() {
    workbench()->setCurrentLayoutIndex((workbench()->currentLayoutIndex() - 1 + workbench()->layouts().size()) % workbench()->layouts().size());
}

void QnWorkbenchActionHandler::at_openInLayoutAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnLayoutResourcePtr layout = parameters.argument<QnLayoutResourcePtr>(Qn::LayoutResourceRole);
    NX_ASSERT(layout, "No layout provided.");
    if (!layout)
        return;

    QPointF position = parameters.argument<QPointF>(Qn::ItemPositionRole);

    int maxItems = (qnSettings->lightMode() & Qn::LightModeSingleItem)
        ? 1
        : qnSettings->maxSceneVideoItems();

    bool adjustAspectRatio = layout->getItems().isEmpty() || !layout->hasCellAspectRatio();

    QnResourceWidgetList widgets = parameters.widgets();
    if (!widgets.empty() && position.isNull() && layout->getItems().empty())
    {
        QHash<QnUuid, QnLayoutItemData> itemDataByUuid;
        for (auto widget: widgets)
        {
            QnLayoutItemData data = widget->item()->data();
            data.flags = Qn::PendingGeometryAdjustment;
            itemDataByUuid[data.uuid] = data;
        }

        /* Generate new UUIDs. */
        for (auto pos = itemDataByUuid.begin(); pos != itemDataByUuid.end(); pos++)
            pos->uuid = QnUuid::createUuid();

        /* Update cross-references. */
        for (auto pos = itemDataByUuid.begin(); pos != itemDataByUuid.end(); pos++)
        {
            if (!pos->zoomTargetUuid.isNull())
                pos->zoomTargetUuid = itemDataByUuid[pos->zoomTargetUuid].uuid;
        }

        /* Add to layout. */
        for (const auto& data: itemDataByUuid)
        {
            if (layout->getItems().size() >= maxItems)
                return;

            layout->addItem(data);
        }
    }
    else
    {
        QnResourceList resources = parameters.resources();
        if (!resources.isEmpty())
        {
            AddToLayoutParams addParams;
            addParams.usePosition = !position.isNull();
            addParams.position = position;

            // Live viewers must not open items on archive position
            if (accessController()->hasGlobalPermission(Qn::GlobalViewArchivePermission))
                addParams.time = parameters.argument<qint64>(Qn::ItemTimeRole, -1);
            addToLayout(layout, resources, addParams);
        }
    }

    auto workbenchLayout = workbench()->currentLayout();
    if (adjustAspectRatio && workbenchLayout->resource() == layout)
    {
        qreal midAspectRatio = 0.0;
        int count = 0;

        if (!widgets.isEmpty())
        {
            /* Here we don't take into account already added widgets. It's ok because
            we can get here only if the layout doesn't have cell aspect ratio, that means
            its widgets don't have aspect ratio too. */

            for (auto widget: widgets)
            {
                if (widget->hasAspectRatio())
                {
                    midAspectRatio += widget->visualAspectRatio();
                    ++count;
                }
            }
        }
        else
        {
            for (auto item: workbenchLayout->items())
            {
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

        if (count > 0)
        {
            midAspectRatio /= count;
            QnAspectRatio cellAspectRatio = QnAspectRatio::closestStandardRatio(midAspectRatio);
            layout->setCellAspectRatio(cellAspectRatio.toFloat());
        }
        else if (workbenchLayout->items().size() > 1)
        {
            layout->setCellAspectRatio(qnGlobals->defaultLayoutCellAspectRatio());
        }
    }
}

void QnWorkbenchActionHandler::at_openInCurrentLayoutAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());
    const auto currentLayout = workbench()->currentLayout();

    // Check if we are in videowall control mode
    QnUuid videoWallItemGuid = currentLayout->data(Qn::VideoWallItemGuidRole).value<QnUuid>();
    if (!videoWallItemGuid.isNull())
    {
        QnVideoWallItemIndex index = qnResPool->getVideoWallItemByUuid(videoWallItemGuid);
        const auto resources = parameters.resources();

        // Displaying message delayed to avoid waiting cursor (see drop_instrument.cpp:245)
        if (!nx::client::messages::VideoWall::checkLocalFiles(mainWindow(), index, resources, true))
            return;
    }

    parameters.setArgument(Qn::LayoutResourceRole, currentLayout->resource());
    menu()->trigger(QnActions::OpenInLayoutAction, parameters);
}

void QnWorkbenchActionHandler::at_openInNewLayoutAction_triggered() {
    menu()->trigger(QnActions::OpenNewTabAction);
    menu()->trigger(QnActions::OpenInCurrentLayoutAction, menu()->currentParameters(sender()));
}

void QnWorkbenchActionHandler::at_openInNewWindowAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnResourceList filtered;
    for (const auto& resource: parameters.resources())
    {
        if (menu()->canTrigger(QnActions::OpenInNewWindowAction, resource))
            filtered << resource;
    }
    if (filtered.isEmpty())
        return;

    openResourcesInNewWindow(filtered);
}

void QnWorkbenchActionHandler::at_openLayoutsAction_triggered() {
    foreach(const QnResourcePtr &resource, menu()->currentParameters(sender()).resources()) {
        QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
        if (!layoutResource)
            continue;

        QnWorkbenchLayout *layout = QnWorkbenchLayout::instance(layoutResource);
        if (layout == NULL) {
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
    if (layouts.isEmpty())
        return;
    openResourcesInNewWindow(layouts);
}

void QnWorkbenchActionHandler::at_openCurrentLayoutInNewWindowAction_triggered() {
    menu()->trigger(QnActions::OpenLayoutsInNewWindowAction, workbench()->currentLayout()->resource());
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

    if (status != 0)
    {
        const auto text = QnDeviceDependentStrings::getNameFromSet(
            QnCameraDeviceStringSet(
                tr("Failed to move %n devices", "", modifiedResources.size()),
                tr("Failed to move %n cameras", "", modifiedResources.size()),
                tr("Failed to move %n I/O Modules", "", modifiedResources.size())),
            modifiedResources);

        const auto extras = tr("Server \"%1\" is not responding.").arg(server->getName());
        QnMessageBox messageBox(QnMessageBoxIcon::Critical,
            text, extras, QDialogButtonBox::Ok, QDialogButtonBox::Ok,
            mainWindow());

        messageBox.addCustomWidget(new QnResourceListView(modifiedResources, &messageBox));
        messageBox.exec();
        return;
    }

    QnVirtualCameraResourceList errorResources; // TODO: #Elric check server cameras
    for (auto itr = modifiedResources.begin(); itr != modifiedResources.end();)
    {
        if (!reply.uniqueIdList.contains((*itr)->getUniqueId()))
        {
            errorResources << *itr;
            itr = modifiedResources.erase(itr);
        }
        else
        {
            ++itr;
        }
    }

    if (!errorResources.empty())
    {
        const auto text = QnDeviceDependentStrings::getNameFromSet(
            QnCameraDeviceStringSet(
                tr("Server \"%1\" cannot access %n devices. Move them anyway?",
                    "", errorResources.size()),
                tr("Server \"%1\" cannot access %n cameras. Move them anyway?",
                    "", errorResources.size()),
                tr("Server \"%1\" cannot access %n I/O modules. Move them anyway?",
                    "", errorResources.size())),
            errorResources).arg(server->getName());

        QnMessageBox messageBox(QnMessageBoxIcon::Warning,
            text, QString(),
            QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
            mainWindow());

        messageBox.addButton(tr("Move"), QDialogButtonBox::YesRole, QnButtonAccent::Standard);
        const auto skipButton = messageBox.addCustomButton(QnMessageBoxCustomButton::Skip,
            QDialogButtonBox::NoRole);
        messageBox.addCustomWidget(new QnResourceListView(errorResources, &messageBox));

        const auto result = messageBox.exec();
        if (result == QDialogButtonBox::Cancel)
            return;

        /* If user is sure, return invalid cameras back to list. */
        if (skipButton != messageBox.clickedButton())
            modifiedResources << errorResources;
    }

    const QnUuid serverId = server->getId();
    qnResourcesChangesManager->saveCameras(modifiedResources,
        [serverId](const QnVirtualCameraResourcePtr &camera)
        {
            camera->setPreferredServerId(serverId);
        });

    qnResourcesChangesManager->saveCamerasCore(modifiedResources,
        [serverId](const QnVirtualCameraResourcePtr &camera)
        {
            camera->setParentId(serverId);
        });
}

void QnWorkbenchActionHandler::at_moveCameraAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnResourceList resources = parameters.resources();
    QnMediaServerResourcePtr server = parameters.argument<QnMediaServerResourcePtr>(Qn::MediaServerResourceRole);
    if (!server)
        return;
    QnVirtualCameraResourceList resourcesToMove;

    foreach(const QnResourcePtr &resource, resources) {
        if (resource->getParentId() == server->getId())
            continue; /* Moving resource into its owner does nothing. */

        QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (!camera)
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

    if (!workbench()->currentLayout()->resource())
        menu()->trigger(QnActions::OpenNewTabAction);

    NX_ASSERT(workbench()->currentLayout()->resource());

    if (workbench()->currentLayout()->resource() &&
        workbench()->currentLayout()->resource()->locked() &&
        !resources.empty() &&
        layouts.empty() &&
        videowalls.empty()) {
        QnGraphicsMessageBox::information(tr("Layout is locked and cannot be changed."));
        return;
    }

    if (!resources.empty())
    {
        parameters.setResources(resources);
        if (!menu()->triggerIfPossible(QnActions::OpenInCurrentLayoutAction, parameters))
            menu()->triggerIfPossible(QnActions::OpenInNewLayoutAction, parameters);
    }

    if (!layouts.empty())
        menu()->trigger(QnActions::OpenAnyNumberOfLayoutsAction, layouts);

    if (!videowalls.empty())
        menu()->trigger(QnActions::OpenVideoWallsReviewAction, videowalls);
}

void QnWorkbenchActionHandler::at_dropResourcesIntoNewLayoutAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnLayoutResourceList layouts = parameters.resources().filtered<QnLayoutResource>();
    QnVideoWallResourceList videowalls = parameters.resources().filtered<QnVideoWallResource>();

    if (layouts.empty() && (videowalls.size() != parameters.resources().size())) /* There are some media in the drop, open new layout. */
        menu()->trigger(QnActions::OpenNewTabAction);

    menu()->trigger(QnActions::DropResourcesAction, parameters);
}

void QnWorkbenchActionHandler::at_delayedDropResourcesAction_triggered() {
    QByteArray data = menu()->currentParameters(sender()).argument<QByteArray>(Qn::SerializedDataRole);
    QDataStream stream(&data, QIODevice::ReadOnly);
    QnMimeData mimeData;
    stream >> mimeData;
    if (stream.status() != QDataStream::Ok || mimeData.formats().empty())
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
    if (stream.status() != QDataStream::Ok || mimeData.formats().empty())
        return;
    m_instantDrops.push_back(mimeData);

    submitInstantDrop();
}

void QnWorkbenchActionHandler::at_openFileAction_triggered() {
    QStringList filters;
    filters << tr("All Supported (*.nov *.avi *.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp *.jpg *.png *.gif *.bmp *.tiff)");
    filters << tr("Video (*.avi *.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp)");
    filters << tr("Pictures (*.jpg *.png *.gif *.bmp *.tiff)");
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

void QnWorkbenchActionHandler::at_openFolderAction_triggered() {
    QString dirName = QnFileDialog::getExistingDirectory(mainWindow(),
        tr("Select folder..."),
        QString(),
        QnCustomFileDialog::directoryDialogOptions());

    if (!dirName.isEmpty())
        menu()->trigger(QnActions::DropResourcesAction, addToResourcePool(dirName));
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

void QnWorkbenchActionHandler::openSystemAdministrationDialog(int page)
{
    QnNonModalDialogConstructor<QnSystemAdministrationDialog> dialogConstructor(
        m_systemAdministrationDialog, mainWindow());
    systemAdministrationDialog()->setCurrentPage(page);
}

void QnWorkbenchActionHandler::openLocalSettingsDialog(int page)
{
    QScopedPointer<QnLocalSettingsDialog> dialog(new QnLocalSettingsDialog(mainWindow()));
    dialog->setCurrentPage(page);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_aboutAction_triggered() {
    QScopedPointer<QnAboutDialog> dialog(new QnAboutDialog(mainWindow()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_businessEventsAction_triggered() {
    menu()->trigger(QnActions::OpenBusinessRulesAction);
}

void QnWorkbenchActionHandler::at_openBusinessRulesAction_triggered()
{
    QnNonModalDialogConstructor<QnBusinessRulesDialog> dialogConstructor(m_businessRulesDialog, mainWindow());

    QStringList filter;
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnVirtualCameraResourceList cameras = parameters.resources().filtered<QnVirtualCameraResource>();
    if (!cameras.isEmpty())
    {
        NX_ASSERT(cameras.size() == 1); // currently filter is not implemented for several cameras
        for (const auto& camera: cameras)
            filter.append(camera->getId().toSimpleString());
    }
    businessRulesDialog()->setFilter(filter.join(L' '));
}

void QnWorkbenchActionHandler::at_webClientAction_triggered()
{
    static const auto kPath = lit("/static/index.html");
    static const auto kFragment = lit("/view");

    const auto server = qnCommon->currentServer();
    if (!server)
        return;

#ifdef WEB_CLIENT_SUPPORTS_PROXY
    openInBrowser(server, kPath, kFragment);
#else
    openInBrowserDirectly(server, kPath, kFragment);
#endif
}

void QnWorkbenchActionHandler::at_webAdminAction_triggered()
{
    static const auto kPath = lit("/static/index.html");
    static const auto kFragment = lit("/server");

    const auto server = menu()->currentParameters(sender()).resource()
        .dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

#ifdef WEB_CLIENT_SUPPORTS_PROXY
    openInBrowser(server, kPath, kFragment);
#else
    openInBrowserDirectly(server, kPath, kFragment);
#endif
}

qint64 QnWorkbenchActionHandler::getFirstBookmarkTimeMs()
{
    static const qint64 kOneDayOffsetMs = 60 * 60 * 24 * 1000;
    static const qint64 kOneYearOffsetMs = kOneDayOffsetMs * 365;
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
    auto cameras = parameters.resources().filtered<QnVirtualCameraResource>();
    QSet<QnUuid> ids;
    for (auto camera: cameras)
        ids << camera->getId();

    // show diagnostics if Issues action was triggered
    if (eventType != QnBusiness::AnyBusinessEvent || !ids.isEmpty())
    {
        businessEventsLogDialog()->disableUpdateData();
        businessEventsLogDialog()->setEventType(eventType);
        businessEventsLogDialog()->setActionType(QnBusiness::DiagnosticsAction);
        auto now = QDateTime::currentMSecsSinceEpoch();
        businessEventsLogDialog()->setDateRange(now, now);
        businessEventsLogDialog()->setCameraList(ids);
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

void QnWorkbenchActionHandler::at_thumbnailsSearchAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnResourcePtr resource = parameters.resource();
    if (!resource)
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
    if (!periods.empty()) {

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

    if (period.durationMs < steps[1])
    {
        QnMessageBox::warning(mainWindow(),
            tr("Too short period selected"),
            tr("Cannot perform Preview Search. Please select a period of 15 seconds or longer."));
        return;
    }

    /* Find best time step. */
    qint64 step = 0;
    for (int i = 0; steps[i] > 0; i++) {
        if (period.durationMs < steps[i] * (maxItems - 2)) { /* -2 is here as we're going to snap period ends to closest step points. */
            step = steps[i];
            break;
        }
    }

    int itemCount = 0;
    if (step == 0) {
        /* No luck? Calculate time step based on the maximal number of items. */
        itemCount = maxItems;

        step = period.durationMs / itemCount;
    }
    else {
        /* In this case we want to adjust the period. */

        if (resource->flags() & Qn::utc) {
            QDateTime startDateTime = QDateTime::fromMSecsSinceEpoch(period.startTimeMs);
            QDateTime endDateTime = QDateTime::fromMSecsSinceEpoch(period.endTimeMs());
            const qint64 dayMSecs = 1000ll * 60 * 60 * 24;

            if (step < dayMSecs) {
                int startMSecs = qFloor(QDateTime(startDateTime.date()).msecsTo(startDateTime), step);
                int endMSecs = qCeil(QDateTime(endDateTime.date()).msecsTo(endDateTime), step);

                startDateTime.setTime(QTime(0, 0, 0, 0));
                startDateTime = startDateTime.addMSecs(startMSecs);

                endDateTime.setTime(QTime(0, 0, 0, 0));
                endDateTime = endDateTime.addMSecs(endMSecs);
            }
            else {
                int stepDays = step / dayMSecs;

                startDateTime.setTime(QTime(0, 0, 0, 0));
                startDateTime.setDate(QDate::fromJulianDay(qFloor(startDateTime.date().toJulianDay(), stepDays)));

                endDateTime.setTime(QTime(0, 0, 0, 0));
                endDateTime.setDate(QDate::fromJulianDay(qCeil(endDateTime.date().toJulianDay(), stepDays)));
            }

            period = QnTimePeriod(startDateTime.toMSecsSinceEpoch(), endDateTime.toMSecsSinceEpoch() - startDateTime.toMSecsSinceEpoch());
        }
        else {
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
    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setId(QnUuid::createUuid());
    layout->setData(Qt::DecorationRole, qnSkin->icon("layouts/preview_search.png"));
    layout->setName(tr("Preview Search for %1").arg(resource->getName()));
    if (context()->user())
        layout->setParentId(context()->user()->getId());

    qint64 time = period.startTimeMs;
    for (int i = 0; i < itemCount; i++) {
        QnTimePeriod localPeriod(time, step);
        QnTimePeriodList localPeriods = periods.intersected(localPeriod);
        qint64 localTime = time;
        if (!localPeriods.empty())
            localTime = qMax(localTime, localPeriods.begin()->startTimeMs);

        QnLayoutItemData item;
        item.flags = Qn::Pinned;
        item.uuid = QnUuid::createUuid();
        item.combinedGeometry = QRect(i % matrixWidth, i / matrixWidth, 1, 1);
        item.resource.id = resource->getId();
        item.resource.uniqueId = resource->getUniqueId();
        item.contrastParams = widget->item()->imageEnhancement();
        item.dewarpingParams = widget->item()->dewarpingParams();
        item.rotation = widget->item()->rotation();
        qnResourceRuntimeDataManager->setLayoutItemData(item.uuid, Qn::ItemPausedRole, true);
        qnResourceRuntimeDataManager->setLayoutItemData(item.uuid, Qn::ItemSliderSelectionRole, localPeriod);
        qnResourceRuntimeDataManager->setLayoutItemData(item.uuid, Qn::ItemSliderWindowRole, period);
        qnResourceRuntimeDataManager->setLayoutItemData(item.uuid, Qn::ItemTimeRole, localTime);
        // set aspect ratio to make thumbnails load in all cases, see #2619
        qnResourceRuntimeDataManager->setLayoutItemData(item.uuid, Qn::ItemAspectRatioRole, desiredItemAspectRatio);
        qnResourceRuntimeDataManager->setLayoutItemData(item.uuid, Qn::TimePeriodsRole, localPeriods);

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

    qnResPool->addResource(layout);
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
        dialog.reset(new QnSessionAware<QnMediaFileSettingsDialog>(mainWindow()));
    else
        dialog.reset(new QnMediaFileSettingsDialog(mainWindow()));

    dialog->updateFromResource(media);
    if (dialog->exec()) {
        dialog->submitToResource(media);
    }
    else {
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
    if (!resource)
        return;

    QScopedPointer<QnCameraDiagnosticsDialog> dialog(new QnCameraDiagnosticsDialog(mainWindow()));
    dialog->setResource(resource);
    dialog->restart();
    dialog->exec();
}

void QnWorkbenchActionHandler::at_serverAddCameraManuallyAction_triggered()
{
    const auto params = menu()->currentParameters(sender());
    const auto server = params.resource().dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    QnNonModalDialogConstructor<QnCameraAdditionDialog> dialogConstructor(m_cameraAdditionDialog, mainWindow());

    QnCameraAdditionDialog* dialog = cameraAdditionDialog();

    if (dialog->server() != server) {
        if (dialog->state() == QnCameraAdditionDialog::Searching
            || dialog->state() == QnCameraAdditionDialog::Adding) {

            const auto result = QnMessageBox::question(mainWindow(),
                tr("Cancel device adding?"), QString(),
                QDialogButtonBox::Yes | QDialogButtonBox::No,
                QDialogButtonBox::No);

            if (result != QDialogButtonBox::Yes)
                return;
        }
        dialog->setServer(server);
    }
}

void QnWorkbenchActionHandler::at_serverLogsAction_triggered()
{
    QnMediaServerResourcePtr server = menu()->currentParameters(sender()).resource().dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    openInBrowser(server, lit("/api/showLog?lines=1000"));
}

void QnWorkbenchActionHandler::at_serverIssuesAction_triggered() {
    menu()->trigger(QnActions::OpenBusinessLogAction,
        QnActionParameters().withArgument(Qn::EventTypeRole, QnBusiness::AnyServerEvent));
}

void QnWorkbenchActionHandler::at_pingAction_triggered()
{
    QString host = menu()->currentParameters(sender()).argument(Qn::TextRole).toString();

    if (nx::network::SocketGlobals::addressResolver().isCloudHostName(host))
        return;

#ifdef Q_OS_WIN
    QString cmd = lit("cmd /C ping %1 -t").arg(host);
    QProcess::startDetached(cmd);
#endif
#ifdef Q_OS_LINUX
    QString cmd = lit("xterm -e ping %1").arg(host);
    QProcess::startDetached(cmd);
#endif
#ifdef Q_OS_MACX
    QnPingDialog *dialog = new QnPingDialog(NULL, Qt::Dialog | Qt::WindowStaysOnTopHint);
    dialog->setHostAddress(host);
    dialog->show();
    dialog->startPings();
#endif

}

void QnWorkbenchActionHandler::at_openInFolderAction_triggered() {
    QnResourcePtr resource = menu()->currentParameters(sender()).resource();
    if (resource.isNull())
        return;

    QnEnvironment::showInGraphicalShell(resource->getUrl());
}

void QnWorkbenchActionHandler::at_deleteFromDiskAction_triggered()
{
    auto resources = menu()->currentParameters(sender()).resources().toSet().toList();

    /**
     * #ynikitenkov According to specs this functionality is disabled
     * Change texts and dialog when it is enabled
     */
    QnMessageBox messageBox(
        QnMessageBoxIcon::Warning,
        tr("Confirm files deleting"),
        tr("Are you sure you want to permanently delete these %n files?", "", resources.size()),
        QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::Yes,
        mainWindow());

    messageBox.addCustomWidget(new QnResourceListView(resources, &messageBox));
    auto result = messageBox.exec();
    if (result != QDialogButtonBox::Yes)
        return;

    QnFileProcessor::deleteLocalResources(resources);
}

bool QnWorkbenchActionHandler::validateResourceName(const QnResourcePtr &resource, const QString &newName) const {
    /* Only users and videowall should be checked. Layouts are checked separately, servers and cameras can have the same name. */
    Qn::ResourceFlags checkedFlags = resource->flags() & (Qn::user | Qn::videowall);
    if (!checkedFlags)
        return true;

    /* Resource cannot have both of these flags at once. */
    NX_ASSERT(checkedFlags == Qn::user || checkedFlags == Qn::videowall);

    foreach(const QnResourcePtr &resource, qnResPool->getResources()) {
        if (!resource->hasFlags(checkedFlags))
            continue;
        if (resource->getName().compare(newName, Qt::CaseInsensitive) != 0)
            continue;

        if (checkedFlags == Qn::user)
            QnMessageBox::warning(mainWindow(), tr("There is another user with the same name"));
        else
            nx::client::messages::VideoWall::anotherVideoWall(mainWindow());

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
        case Qn::SharedLayoutNode:
        case Qn::SharedResourceNode:
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

        } while (!validateResourceName(resource, name));
    }

    if (name == oldName)
        return;

    if (QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>())
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
        qnResourcesChangesManager->saveVideoWall(videowall, [name](const QnVideoWallResourcePtr &videowall) { videowall->setName(name); });
    }
    else if (QnWebPageResourcePtr webPage = resource.dynamicCast<QnWebPageResource>())
    {
        qnResourcesChangesManager->saveWebPage(webPage, [name](const QnWebPageResourcePtr &webPage) {  webPage->setName(name); });
    }
    else
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Invalid resource type to rename");
    }
}

void QnWorkbenchActionHandler::at_removeFromServerAction_triggered()
{
    QnResourceList resources = menu()->currentParameters(sender()).resources();

    /* Layouts will be removed in their own handler. Also separately check each resource. */
    resources = resources.filtered(
        [this](const QnResourcePtr& resource)
        {
            return menu()->canTrigger(QnActions::RemoveFromServerAction, resource)
                && !resource->hasFlags(Qn::layout);
        });

    if (nx::client::messages::Resources::deleteResources(mainWindow(), resources))
        qnResourcesChangesManager->deleteResources(resources);
}

void QnWorkbenchActionHandler::closeApplication(bool force) {
    /* Try close, if force - exit anyway. */
    if (!context()->instance<QnWorkbenchStateManager>()->tryClose(force) && !force)
        return;

    menu()->trigger(QnActions::BeforeExitAction);
    context()->setClosingDown(true);
    qApp->exit(0);
    applauncher::scheduleProcessKill(QCoreApplication::applicationPid(), PROCESS_TERMINATE_TIMEOUT);
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
    if (!widget)
        return;

    QnNonModalDialogConstructor<QnAdjustVideoDialog> dialogConstructor(m_adjustVideoDialog, mainWindow());
    adjustVideoDialog()->setWidget(widget);
}

void QnWorkbenchActionHandler::at_createZoomWindowAction_triggered() {
    QnActionParameters params = menu()->currentParameters(sender());

    QnMediaResourceWidget *widget = params.widget<QnMediaResourceWidget>();
    if (!widget)
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

        if (!accessController()->hasPermissions(workbench()->currentLayout()->resource(), Qn::EditLayoutSettingsPermission))
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
    connect(progressDialog, &QnProgressDialog::canceled, progressDialog, &QObject::deleteLater);

    QnActionParameters parameters = menu()->currentParameters(sender());

    QnAppServerImageCache *cache = new QnAppServerImageCache(this);
    cache->setProperty(uploadingImageARPropertyName, parameters.widget()->aspectRatio());
    connect(cache, &QnAppServerImageCache::fileUploaded, progressDialog, &QObject::deleteLater);
    connect(cache, &QnAppServerImageCache::fileUploaded, cache, &QObject::deleteLater);
    connect(cache, &QnAppServerImageCache::fileUploaded, this, [this, checkCondition](const QString &filename, QnAppServerFileCache::OperationResult status) {
        if (!checkCondition())
            return;

        if (status == QnAppServerFileCache::OperationResult::sizeLimitExceeded)
        {
            const auto maxFileSize = QnAppServerFileCache::maximumFileSize() / (1024 * 1024);
            QnMessageBox::warning(mainWindow(),
                tr("Image too big"),
                tr("Maximum size is %1 MB.").arg(maxFileSize));
            return;
        }

        if (status != QnAppServerFileCache::OperationResult::ok)
        {
            QnMessageBox::critical(mainWindow(), tr("Failed to upload image"));
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
    if (qFuzzyIsNull(layout->backgroundOpacity()))
        layout->setBackgroundOpacity(0.7);

    wlayout->centralizeItems();
    QRect brect = wlayout->boundingRect();

    int minWidth = qMax(brect.width(), qnGlobals->layoutBackgroundMinSize().width());
    int minHeight = qMax(brect.height(), qnGlobals->layoutBackgroundMinSize().height());

    qreal cellAspectRatio = qnGlobals->defaultLayoutCellAspectRatio();
    if (layout->hasCellAspectRatio())
    {
        const auto spacing = layout->cellSpacing();
        qreal cellWidth = 1.0 + spacing;
        qreal cellHeight = 1.0 / layout->cellAspectRatio() + spacing;
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

    }
    else {
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

    // TODO: #Elric totally evil copypasta and hacky workaround.
    bool enabled =
        context()->instance<QnWorkbenchScheduleWatcher>()->isScheduleEnabled() &&
        accessController()->hasGlobalPermission(Qn::GlobalAdminPermission);
    action(QnActions::TogglePanicModeAction)->setEnabled(enabled);
}

void QnWorkbenchActionHandler::at_scheduleWatcher_scheduleEnabledChanged() {
    // TODO: #Elric totally evil copypasta and hacky workaround.
    bool enabled =
        context()->instance<QnWorkbenchScheduleWatcher>()->isScheduleEnabled() &&
        accessController()->hasGlobalPermission(Qn::GlobalAdminPermission);

    action(QnActions::TogglePanicModeAction)->setEnabled(enabled);
    if (!enabled)
        action(QnActions::TogglePanicModeAction)->setChecked(false);
}

void QnWorkbenchActionHandler::at_togglePanicModeAction_toggled(bool checked) {
    QnMediaServerResourceList resources = qnResPool->getAllServers(Qn::AnyStatus);

    foreach(QnMediaServerResourcePtr resource, resources)
    {
        bool isPanicMode = resource->getPanicMode() != Qn::PM_None;
        if (isPanicMode != checked) {
            Qn::PanicMode val = Qn::PM_None;
            if (checked)
                val = Qn::PM_User;
            resource->setPanicMode(val);
            propertyDictionary->saveParamsAsync(resource->getId());
        }
    }
}

void QnWorkbenchActionHandler::at_toggleTourAction_toggled(bool checked) {
    if (!checked) {
        m_tourTimer->stop();
        context()->workbench()->setItem(Qn::ZoomedRole, NULL);
    }
    else {
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
    std::sort(items.begin(), items.end(), ItemPositionCmp());

    if (items.empty()) {
        action(QnActions::ToggleTourModeAction)->setChecked(false);
        return;
    }

    QnWorkbenchItem *item = context()->workbench()->item(Qn::ZoomedRole);
    if (item) {
        item = items[(items.indexOf(item) + 1) % items.size()];
    }
    else {
        item = items[0];
    }
    context()->workbench()->setItem(Qn::ZoomedRole, item);
}

void QnWorkbenchActionHandler::at_workbench_itemChanged(Qn::ItemRole role) {
    if (!workbench()->item(Qn::ZoomedRole))
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

void QnWorkbenchActionHandler::at_browseUrlAction_triggered() {
    QString url = menu()->currentParameters(sender()).argument<QString>(Qn::UrlRole);
    if (url.isEmpty())
        return;

    QDesktopServices::openUrl(QUrl::fromUserInput(url));
}

void QnWorkbenchActionHandler::at_versionMismatchMessageAction_triggered()
{
    if (qnCommon->isReadOnly())
        return;

    if (qnRuntime->ignoreVersionMismatch())
        return;

    if (qnClientShowOnce->testFlag(kVersionMismatchShowOnceKey))
        return;

    QnWorkbenchVersionMismatchWatcher *watcher = context()->instance<QnWorkbenchVersionMismatchWatcher>();
    if (!watcher->hasMismatches())
        return;

    QnSoftwareVersion latestVersion = watcher->latestVersion();
    QnSoftwareVersion latestMsVersion = watcher->latestVersion(Qn::ServerComponent);

    // if some component is newer than the newest mediaserver, focus on its version
    if (QnWorkbenchVersionMismatchWatcher::versionMismatches(latestVersion, latestMsVersion))
        latestMsVersion = latestVersion;

    QStringList messageParts;
    for (const QnAppInfoMismatchData &data : watcher->mismatchData())
    {
        QString componentName;
        switch (data.component)
        {
        case Qn::ClientComponent:
            componentName = tr("Client");
            break;
        case Qn::ServerComponent:
        {
            QnMediaServerResourcePtr resource = data.resource.dynamicCast<QnMediaServerResource>();
            componentName = resource ? QnResourceDisplayInfo(resource).toString(Qn::RI_WithUrl) : tr("Server");
            break;
        }
        default:
            break;
        }
        NX_ASSERT(!componentName.isEmpty());
        if (componentName.isEmpty())
            continue;

        bool updateRequested = (data.component == Qn::ServerComponent) &&
            QnWorkbenchVersionMismatchWatcher::versionMismatches(data.version, latestMsVersion, true);

        const QString version = (updateRequested
            ? setWarningStyleHtml(data.version.toString())
            : data.version.toString());

        /* Consistency with 'About' dialog. */
        QString component = lit("%1: %2").arg(componentName, version);
        messageParts << component;
    }

    messageParts << QString();
    messageParts << tr("Please update all components to the version %1").arg(latestMsVersion.toString());

    const QString extras = messageParts.join(lit("<br/>"));
    QScopedPointer<QnSessionAwareMessageBox> messageBox(
        new QnSessionAwareMessageBox(mainWindow()));
    messageBox->setIcon(QnMessageBoxIcon::Warning);
    messageBox->setText(tr("Components of System have different versions:"));
    messageBox->setInformativeText(extras);
    messageBox->setCheckBoxEnabled();

    const auto updateButton = messageBox->addButton(
        tr("Update..."), QDialogButtonBox::AcceptRole, QnButtonAccent::Standard);
    messageBox->addButton(
        tr("Skip"), QDialogButtonBox::RejectRole);

    messageBox->exec();

    if (messageBox->isChecked())
        qnClientShowOnce->setFlag(kVersionMismatchShowOnceKey);

    if (messageBox->clickedButton() == updateButton)
        menu()->trigger(QnActions::SystemUpdateAction);
}

void QnWorkbenchActionHandler::at_betaVersionMessageAction_triggered()
{
    if (context()->closingDown())
        return;

    if (qnClientShowOnce->testFlag(kBetaVersionShowOnceKey))
        return;

    QnMessageBox dialog(QnMessageBoxIcon::Information,
        tr("Beta version %1").arg(QnAppInfo::applicationVersion()),
        tr("Some functionality may be unavailable or not working properly."),
        QDialogButtonBox::Ok, QDialogButtonBox::Ok, mainWindow());

    dialog.setCheckBoxEnabled();
    dialog.exec();

    if (dialog.isChecked())
        qnClientShowOnce->setFlag(kBetaVersionShowOnceKey);
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

    QnMessageBox::information(mainWindow(),
        tr("System sends anonymous usage statistics"),
        tr("It will be used by software development team to improve your user experience.")
            + L'\n' + tr("To disable it, go to System Administration dialog."));

    qnGlobalSettings->setStatisticsAllowed(true);
    qnGlobalSettings->synchronizeNow();
}

void QnWorkbenchActionHandler::at_queueAppRestartAction_triggered()
{

    auto tryToRestartClient = []
        {
            using namespace applauncher;
            if (!checkOnline())
                return false;

            /* Try to run applauncher if it is not running. */
            if (!checkOnline())
                return false;

            const auto result = restartClient();
            if (result == api::ResultType::ok)
                return true;

            static const int kMaxTries = 5;
            for (int i = 0; i < kMaxTries; ++i)
            {
                QThread::msleep(100);
                qApp->processEvents();
                if (restartClient() == api::ResultType::ok)
                    return true;
            }
            return false;
        };

    if (!tryToRestartClient())
    {
        QnConnectionDiagnosticsHelper::failedRestartClientMessage(mainWindow());
        return;
    }
    menu()->trigger(QnActions::DelayedForcedExitAction);
}

void QnWorkbenchActionHandler::at_selectTimeServerAction_triggered()
{
    openSystemAdministrationDialog(QnSystemAdministrationDialog::TimeServerSelection);
}

void QnWorkbenchActionHandler::openInBrowserDirectly(const QnMediaServerResourcePtr& server,
    const QString& path, const QString& fragment)
{
    // TODO: #akolesnikov #3.1 VMS-2806 deprecate this method, always use openInBrowser()

    if (nx::network::isCloudServer(server))
        return;

    QUrl url(server->getApiUrl());
    url.setUserName(QString());
    url.setPassword(QString());
    url.setScheme(lit("http"));
    url.setPath(path);
    url.setFragment(fragment);
    url = QnNetworkProxyFactory::instance()->urlToResource(url, server, lit("proxy"));
    QDesktopServices::openUrl(url);
}

void QnWorkbenchActionHandler::openInBrowser(const QnMediaServerResourcePtr& server,
    const QString& path, const QString& fragment)
{
    if (!server || !context()->user())
        return;
    // path may contains path + url query params
    // TODO: #akolesnikov #3.1 VMS-2806
    QUrl serverUrl(server->getApiUrl().toString() + path);
    serverUrl.setFragment(fragment);

    QUrl proxyUrl = QnNetworkProxyFactory::instance()->urlToResource(serverUrl, server);
    proxyUrl.setPath(lit("/api/getNonce"));

    if (m_serverRequests.find(proxyUrl) == m_serverRequests.end())
    {
        // No other requests to this proxy, so we have to get nonce by ourself.
        auto reply = new QnAsyncHttpClientReply(nx_http::AsyncHttpClient::create(), this);
        connect(
            reply, &QnAsyncHttpClientReply::finished,
            this, &QnWorkbenchActionHandler::at_nonceReceived);

        // TODO: Think of preloader in case of user complains about delay.
        reply->asyncHttpClient()->doGet(proxyUrl);
    }

    m_serverRequests.emplace(proxyUrl, ServerRequest{server, serverUrl});
}

void QnWorkbenchActionHandler::at_nonceReceived(QnAsyncHttpClientReply *reply)
{
    std::unique_ptr<QnAsyncHttpClientReply> replyGuard(reply);
    const auto nonceUrl = reply->url();
    auto range = m_serverRequests.equal_range(nonceUrl);
    if (range.first == range.second)
        return;

    std::vector<ServerRequest> requests;
    for (auto it = range.first; it != range.second; ++it)
        requests.push_back(std::move(it->second));

    m_serverRequests.erase(range.first, range.second);

    QnJsonRestResult result;
    NonceReply auth;
    if (!QJson::deserialize(reply->data(), &result) || !QJson::deserialize(result.reply, &auth))
    {
        QnMessageBox::critical(mainWindow(), tr("Failed to open server web page"));

        return;
    }

    for (const auto& request: requests)
    {
        const auto appserverUrl = QnAppServerConnectionFactory::url();
        const auto authParam = createHttpQueryAuthParam(
            appserverUrl.userName(), appserverUrl.password(),
            auth.realm, nx_http::Method::GET, auth.nonce.toUtf8());

        QUrl targetUrl(request.url);
        QUrlQuery urlQuery(targetUrl);
        urlQuery.addQueryItem(lit("auth"), QLatin1String(authParam));
        targetUrl.setQuery(urlQuery);

        targetUrl = QnNetworkProxyFactory::instance()->urlToResource(targetUrl, request.server);

        auto gateway = nx::cloud::gateway::VmsGatewayEmbeddable::instance();
        targetUrl = QUrl(lit("http://%1/%2:%3:%4%5?%6")
            .arg(gateway->endpoint().toString()).arg(targetUrl.scheme())
            .arg(targetUrl.host()).arg(targetUrl.port())
            .arg(targetUrl.path()).arg(targetUrl.query()));

        QDesktopServices::openUrl(targetUrl);
    }
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
