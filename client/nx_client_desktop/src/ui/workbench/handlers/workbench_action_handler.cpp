#include "workbench_action_handler.h"

#include <cassert>

#include <QtCore/QProcess>

#include <QtGui/QDesktopServices>
#include <QtGui/QImage>
#include <QtGui/QImageWriter>

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWhatsThis>

#include <api/network_proxy_factory.h>
#include <api/global_settings.h>
#include <api/server_rest_connection.h>

#include <nx/vms/event/action_parameters.h>

#include <camera/resource_display.h>
#include <camera/cam_display.h>
#include <camera/camera_data_manager.h>

#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client/desktop_client_message_processor.h>
#include <client/client_show_once_settings.h>
#include <client/client_module.h>

#include <common/common_module.h>

#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/layout_tour_manager.h>

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

#include <client_core/client_core_module.h>

#include <nx/client/core/utils/geometry.h>

#include <nx/client/desktop/ui/messages/resources_messages.h>
#include <nx/client/desktop/ui/messages/videowall_messages.h>
#include <nx/client/desktop/ui/messages/local_files_messages.h>

#include <nx/network/http/http_types.h>
#include <nx/network/socket_global.h>
#include <nx/network/address_resolver.h>

#include <nx/streaming/archive_stream_reader.h>

#include <network/cloud_url_validator.h>

#include <core/resource/avi/avi_resource.h>
#include <core/storage/file_storage/layout_storage_resource.h>

#include <platform/environment.h>

#include <recording/time_period_list.h>

#include <nx/client/desktop/ui/actions/action_manager.h>

#include <ui/dialogs/about_dialog.h>
#include <ui/dialogs/connection_testing_dialog.h>
#include <ui/dialogs/local_settings_dialog.h>
#include <ui/dialogs/camera_addition_dialog.h>
#include <ui/dialogs/common/input_dialog.h>
#include <ui/dialogs/common/progress_dialog.h>
#include <ui/dialogs/business_rules_dialog.h>
#include <ui/dialogs/failover_priority_dialog.h>
#include <ui/dialogs/backup_cameras_dialog.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_dialog.h>
#include <ui/dialogs/camera_diagnostics_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/dialogs/ping_dialog.h>
#include <ui/dialogs/system_administration_dialog.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>
#include <ui/dialogs/camera_password_change_dialog.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>

#include <nx/client/desktop/resource_properties/media_file/media_file_settings_dialog.h>
#include <nx/client/desktop/resource_properties/camera/camera_settings_tab.h>

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
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_state_manager.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_welcome_screen.h>

#include <ui/workbench/handlers/workbench_layouts_handler.h>    // TODO: #GDM dependencies

#include <ui/workbench/watchers/workbench_user_watcher.h>
#include <ui/workbench/watchers/workbench_panic_watcher.h>
#include <ui/workbench/watchers/workbench_schedule_watcher.h>
#include <ui/workbench/watchers/workbench_update_watcher.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>
#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>
#include <ui/workbench/watchers/workbench_bookmarks_watcher.h>
#include <nx/client/desktop/utils/server_image_cache.h>

#include <utils/applauncher_utils.h>
#include <nx/client/desktop/utils/local_file_cache.h>
#include <utils/common/delete_later.h>
#include <nx/client/desktop/utils/mime_data.h>
#include <utils/common/event_processors.h>
#include <nx/utils/string.h>
#include <utils/common/delayed.h>
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
#include <nx/client/desktop/ui/workbench/layouts/layout_factory.h>
#include <nx/utils/app_info.h>

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

#include <nx/client/desktop/ui/main_window.h>
#include <nx/client/desktop/ui/dialogs/device_addition_dialog.h>

using nx::client::core::Geometry;

namespace {

/* Beta version message. */
static const QString kBetaVersionShowOnceKey(lit("BetaVersion"));

/* Asking for update all outdated servers to the last version. */
static const QString kVersionMismatchShowOnceKey(lit("VersionMismatch"));

const char* uploadingImageARPropertyName = "_qn_uploadingImageARPropertyName";

} // namespace

//!time that is given to process to exit. After that, applauncher (if present) will try to terminate it
static const quint32 PROCESS_TERMINATE_TIMEOUT = 15000;


namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

using utils::UnityLauncherWorkaround;

// -------------------------------------------------------------------------- //
// ActionHandler
// -------------------------------------------------------------------------- //
ActionHandler::ActionHandler(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_delayedDropGuard(false)
{
    connect(context(), &QnWorkbenchContext::userChanged, this,
        &ActionHandler::at_context_userChanged);

    connect(workbench(), SIGNAL(itemChanged(Qn::ItemRole)), this, SLOT(at_workbench_itemChanged(Qn::ItemRole)));
    connect(workbench(), SIGNAL(cellSpacingChanged()), this, SLOT(at_workbench_cellSpacingChanged()));

    connect(action(action::AboutAction), SIGNAL(triggered()), this, SLOT(at_aboutAction_triggered()));
    connect(action(action::OpenFileAction), SIGNAL(triggered()), this, SLOT(at_openFileAction_triggered()));
    connect(action(action::OpenFolderAction), SIGNAL(triggered()), this, SLOT(at_openFolderAction_triggered()));

    connect(qnGlobalSettings, &QnGlobalSettings::maxSceneItemsChanged, this,
        [this]
        {
            qnRuntime->setMaxSceneItemsOverride(qnGlobalSettings->maxSceneItemsOverride());
        });

    // local settings
    connect(action(action::PreferencesGeneralTabAction), &QAction::triggered, this,
        [this] { openLocalSettingsDialog(QnLocalSettingsDialog::GeneralPage); },
        Qt::QueuedConnection);
    connect(action(action::PreferencesNotificationTabAction), &QAction::triggered, this,
        [this] { openLocalSettingsDialog(QnLocalSettingsDialog::NotificationsPage); },
        Qt::QueuedConnection);

    // system administration
    connect(action(action::SystemAdministrationAction), &QAction::triggered, this,
        [this] { openSystemAdministrationDialog(QnSystemAdministrationDialog::GeneralPage); });
    connect(action(action::PreferencesLicensesTabAction), &QAction::triggered, this,
        [this] { openSystemAdministrationDialog(QnSystemAdministrationDialog::LicensesPage); });
    connect(action(action::PreferencesSmtpTabAction), &QAction::triggered, this,
        [this] { openSystemAdministrationDialog(QnSystemAdministrationDialog::SmtpPage); });
    connect(action(action::PreferencesCloudTabAction), &QAction::triggered, this,
        [this]{ openSystemAdministrationDialog(QnSystemAdministrationDialog::CloudManagement); });
    connect(action(action::SystemUpdateAction), &QAction::triggered, this,
        [this] { openSystemAdministrationDialog(QnSystemAdministrationDialog::UpdatesPage); });
    connect(action(action::UserManagementAction), &QAction::triggered, this,
        [this] { openSystemAdministrationDialog(QnSystemAdministrationDialog::UserManagement); });

    connect(action(action::BusinessEventsAction), SIGNAL(triggered()), this, SLOT(at_businessEventsAction_triggered()));
    connect(action(action::OpenBusinessRulesAction), SIGNAL(triggered()), this, SLOT(at_openBusinessRulesAction_triggered()));
    connect(action(action::OpenFailoverPriorityAction), &QAction::triggered, this, &ActionHandler::openFailoverPriorityDialog);
    connect(action(action::OpenBackupCamerasAction), &QAction::triggered, this, &ActionHandler::openBackupCamerasDialog);
    connect(action(action::OpenBookmarksSearchAction), SIGNAL(triggered()), this, SLOT(at_openBookmarksSearchAction_triggered()));
    connect(action(action::OpenBusinessLogAction), SIGNAL(triggered()), this, SLOT(at_openBusinessLogAction_triggered()));
    connect(action(action::OpenAuditLogAction), SIGNAL(triggered()), this, SLOT(at_openAuditLogAction_triggered()));
    connect(action(action::CameraListAction), SIGNAL(triggered()), this, SLOT(at_cameraListAction_triggered()));
    connect(action(action::CameraListByServerAction), SIGNAL(triggered()), this, SLOT(at_cameraListAction_triggered()));

    connect(action(action::WebClientAction), &QAction::triggered, this,
        &ActionHandler::at_webClientAction_triggered);
    connect(action(action::WebAdminAction), &QAction::triggered, this,
        &ActionHandler::at_webAdminAction_triggered);

    connect(action(action::NextLayoutAction), &QAction::triggered, this,
        &ActionHandler::at_nextLayoutAction_triggered);
    connect(action(action::PreviousLayoutAction), &QAction::triggered, this,
        &ActionHandler::at_previousLayoutAction_triggered);

    connect(action(action::OpenInLayoutAction), SIGNAL(triggered()), this, SLOT(at_openInLayoutAction_triggered()));
    connect(action(action::OpenInCurrentLayoutAction), SIGNAL(triggered()), this, SLOT(at_openInCurrentLayoutAction_triggered()));
    connect(action(action::OpenInNewTabAction), &QAction::triggered, this,
        &ActionHandler::at_openInNewTabAction_triggered);
    connect(action(action::OpenInNewWindowAction), SIGNAL(triggered()), this, SLOT(at_openInNewWindowAction_triggered()));

    connect(action(action::OpenCurrentLayoutInNewWindowAction), SIGNAL(triggered()), this, SLOT(at_openCurrentLayoutInNewWindowAction_triggered()));
    connect(action(action::OpenNewWindowAction), SIGNAL(triggered()), this, SLOT(at_openNewWindowAction_triggered()));
    connect(action(action::ReviewLayoutTourInNewWindowAction), &QAction::triggered, this,
        &ActionHandler::at_reviewLayoutTourInNewWindowAction_triggered);

    connect(action(action::ChangeDefaultCameraPasswordAction), &QAction::triggered,
        this, &ActionHandler::at_changeDefaultCameraPassword_triggered);

    connect(action(action::MediaFileSettingsAction), &QAction::triggered, this, &ActionHandler::at_mediaFileSettingsAction_triggered);
    connect(action(action::CameraIssuesAction), SIGNAL(triggered()), this, SLOT(at_cameraIssuesAction_triggered()));
    connect(action(action::CameraBusinessRulesAction), SIGNAL(triggered()), this, SLOT(at_cameraBusinessRulesAction_triggered()));
    connect(action(action::CameraDiagnosticsAction), SIGNAL(triggered()), this, SLOT(at_cameraDiagnosticsAction_triggered()));
    connect(action(action::ServerAddCameraManuallyAction), SIGNAL(triggered()), this, SLOT(at_serverAddCameraManuallyAction_triggered()));
    connect(action(action::PingAction), SIGNAL(triggered()), this, SLOT(at_pingAction_triggered()));
    connect(action(action::ServerLogsAction), SIGNAL(triggered()), this, SLOT(at_serverLogsAction_triggered()));
    connect(action(action::ServerIssuesAction), SIGNAL(triggered()), this, SLOT(at_serverIssuesAction_triggered()));
    connect(action(action::OpenInFolderAction), SIGNAL(triggered()), this, SLOT(at_openInFolderAction_triggered()));
    connect(action(action::DeleteFromDiskAction), SIGNAL(triggered()), this, SLOT(at_deleteFromDiskAction_triggered()));
    connect(action(action::RemoveFromServerAction), SIGNAL(triggered()), this, SLOT(at_removeFromServerAction_triggered()));
    connect(action(action::RenameResourceAction), &QAction::triggered, this,
        &ActionHandler::at_renameAction_triggered);
    connect(action(action::DropResourcesAction), SIGNAL(triggered()), this, SLOT(at_dropResourcesAction_triggered()));
    connect(action(action::DelayedDropResourcesAction), SIGNAL(triggered()), this, SLOT(at_delayedDropResourcesAction_triggered()));
    connect(action(action::InstantDropResourcesAction), SIGNAL(triggered()), this, SLOT(at_instantDropResourcesAction_triggered()));
    connect(action(action::MoveCameraAction), SIGNAL(triggered()), this, SLOT(at_moveCameraAction_triggered()));
    connect(action(action::AdjustVideoAction), SIGNAL(triggered()), this, SLOT(at_adjustVideoAction_triggered()));
    connect(action(action::ExitAction), &QAction::triggered, this, &ActionHandler::closeApplication);
    connect(action(action::ThumbnailsSearchAction), SIGNAL(triggered()), this, SLOT(at_thumbnailsSearchAction_triggered()));
    connect(action(action::CreateZoomWindowAction), SIGNAL(triggered()), this, SLOT(at_createZoomWindowAction_triggered()));

    connect(action(action::AddDeviceManuallyAction), &QAction::triggered,
        this, &ActionHandler::at_addDeviceManually_triggered);

    connect(action(action::ConvertCameraToEntropix), &QAction::triggered,
        this, &ActionHandler::at_convertCameraToEntropix_triggered);

    connect(action(action::SetCurrentLayoutItemSpacingNoneAction), &QAction::triggered, this,
        [this]
        {
            setCurrentLayoutCellSpacing(Qn::CellSpacing::None);
        });

    connect(action(action::SetCurrentLayoutItemSpacingSmallAction), &QAction::triggered, this,
        [this]
        {
            setCurrentLayoutCellSpacing(Qn::CellSpacing::Small);
        });

    connect(action(action::SetCurrentLayoutItemSpacingMediumAction), &QAction::triggered, this,
        [this]
        {
            setCurrentLayoutCellSpacing(Qn::CellSpacing::Medium);
        });

    connect(action(action::SetCurrentLayoutItemSpacingLargeAction), &QAction::triggered, this,
        [this]
        {
            setCurrentLayoutCellSpacing(Qn::CellSpacing::Large);
        });

    connect(action(action::Rotate0Action), &QAction::triggered, this, [this] { rotateItems(0); });
    connect(action(action::Rotate90Action), &QAction::triggered, this, [this] { rotateItems(90); });
    connect(action(action::Rotate180Action), &QAction::triggered, this, [this] { rotateItems(180); });
    connect(action(action::Rotate270Action), &QAction::triggered, this, [this] { rotateItems(270); });
    connect(action(action::SetAsBackgroundAction), SIGNAL(triggered()), this, SLOT(at_setAsBackgroundAction_triggered()));
    connect(action(action::WhatsThisAction), SIGNAL(triggered()), this, SLOT(at_whatsThisAction_triggered()));
    connect(action(action::EscapeHotkeyAction), SIGNAL(triggered()), this, SLOT(at_escapeHotkeyAction_triggered()));
    connect(action(action::BrowseUrlAction), SIGNAL(triggered()), this, SLOT(at_browseUrlAction_triggered()));
    connect(action(action::VersionMismatchMessageAction), &QAction::triggered, this, &ActionHandler::at_versionMismatchMessageAction_triggered);
    connect(action(action::BetaVersionMessageAction), SIGNAL(triggered()), this, SLOT(at_betaVersionMessageAction_triggered()));
    connect(action(action::AllowStatisticsReportMessageAction), &QAction::triggered, this, [this] { checkIfStatisticsReportAllowed(); });

    connect(action(action::QueueAppRestartAction), SIGNAL(triggered()), this, SLOT(at_queueAppRestartAction_triggered()));
    connect(action(action::SelectTimeServerAction), SIGNAL(triggered()), this, SLOT(at_selectTimeServerAction_triggered()));

    connect(action(action::TogglePanicModeAction), SIGNAL(toggled(bool)), this, SLOT(at_togglePanicModeAction_toggled(bool)));

    connect(context()->instance<QnWorkbenchScheduleWatcher>(), SIGNAL(scheduleEnabledChanged()), this, SLOT(at_scheduleWatcher_scheduleEnabledChanged()));

    /* Connect through lambda to handle forced parameter. */
    connect(action(action::DelayedForcedExitAction), &QAction::triggered, this, [this] {  closeApplication(true);    }, Qt::QueuedConnection);

    connect(action(action::BeforeExitAction), &QAction::triggered, this, &ActionHandler::at_beforeExitAction_triggered);

    connect(action(action::OpenNewSceneAction), &QAction::triggered, this, &ActionHandler::at_openNewScene_triggered);

    /* Run handlers that update state. */
    at_scheduleWatcher_scheduleEnabledChanged();
}

ActionHandler::~ActionHandler()
{
    deleteDialogs();
}

void ActionHandler::addToLayout(const QnLayoutResourcePtr &layout, const QnResourcePtr &resource, const AddToLayoutParams &params) const
{
    if (!layout)
        return;

    if (qnSettings->lightMode() & Qn::LightModeSingleItem) {
        while (!layout->getItems().isEmpty())
            layout->removeItem(*(layout->getItems().begin()));
    }

    if (layout->getItems().size() >= qnRuntime->maxSceneItems())
        return;

    if (!menu()->canTrigger(action::OpenInLayoutAction, action::Parameters(resource)
        .withArgument(Qn::LayoutResourceRole, layout)))
    {
        return;
    }

    QnLayoutItemData data;
    data.resource.id = resource->getId();
    if (resource->hasFlags(Qn::local_media))
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

    if (!params.zoomWindowRectangleVisible)
    {
        qnResourceRuntimeDataManager->setLayoutItemData(
            data.uuid, Qn::ItemZoomWindowRectangleVisibleRole, false);
    }

    if (params.usePosition)
        data.combinedGeometry = QRectF(params.position, params.position); /* Desired position is encoded into a valid rect. */
    else
        data.combinedGeometry = QRectF(QPointF(0.5, 0.5), QPointF(-0.5, -0.5)); /* The fact that any position is OK is encoded into an invalid rect. */
    layout->addItem(data);
}

void ActionHandler::addToLayout(const QnLayoutResourcePtr &layout, const QnResourceList &resources, const AddToLayoutParams &params) const {
    foreach(const QnResourcePtr &resource, resources)
        addToLayout(layout, resource, params);
}

void ActionHandler::addToLayout(const QnLayoutResourcePtr &layout, const QList<QnMediaResourcePtr>& resources, const AddToLayoutParams &params) const {
    foreach(const QnMediaResourcePtr &resource, resources)
        addToLayout(layout, resource->toResourcePtr(), params);
}

void ActionHandler::addToLayout(const QnLayoutResourcePtr &layout, const QList<QString> &files, const AddToLayoutParams &params) const {
    addToLayout(layout, addToResourcePool(files), params);
}

QnResourceList ActionHandler::addToResourcePool(const QList<QString> &files) const {
    return QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(files));
}

QnResourceList ActionHandler::addToResourcePool(const QString &file) const {
    return QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(file));
}

void ActionHandler::openResourcesInNewWindow(const QnResourceList &resources)
{
    QStringList arguments;

    if (!resources.isEmpty())
    {
        if (context()->user())
            arguments << QLatin1String("--delayed-drop");
        else
            arguments << QLatin1String("--instant-drop");

        MimeData data;
        data.setResources(resources);
        arguments << data.serialized();
    }

    openNewWindow(arguments);
}

void ActionHandler::openNewWindow(const QStringList &args) {
    QStringList arguments = args;
    arguments << lit("--no-single-application");
    arguments << lit("--no-version-mismatch-check");

    if (context()->user())
    {
        arguments << lit("--auth");
        arguments << QnStartupParameters::createAuthenticationString(
            commonModule()->currentUrl());
    }

    if (mainWindowWidget())
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

void ActionHandler::rotateItems(int degrees) {
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();
    if (!widgets.empty()) {
        foreach(const QnResourceWidget *widget, widgets)
            widget->item()->setRotation(degrees);
    }
}

void ActionHandler::setCurrentLayoutCellSpacing(Qn::CellSpacing spacing)
{
    // TODO: #GDM #3.1 move out these actions to separate CurrentLayoutHandler
    // There at_workbench_cellSpacingChanged will also use this method
    auto actionId = [spacing]
        {
            switch (spacing)
            {
                case Qn::CellSpacing::None:
                    return action::SetCurrentLayoutItemSpacingNoneAction;
                case Qn::CellSpacing::Small:
                    return action::SetCurrentLayoutItemSpacingSmallAction;
                case Qn::CellSpacing::Medium:
                    return action::SetCurrentLayoutItemSpacingMediumAction;
                case Qn::CellSpacing::Large:
                    return action::SetCurrentLayoutItemSpacingLargeAction;
            }
            NX_ASSERT(false);
            return action::SetCurrentLayoutItemSpacingSmallAction;
        };

    if (auto layout = workbench()->currentLayout()->resource())
    {
        layout->setCellSpacing(QnWorkbenchLayout::cellSpacingValue(spacing));
        action(actionId())->setChecked(true);
    }
}

QnBusinessRulesDialog *ActionHandler::businessRulesDialog() const
{
    return m_businessRulesDialog.data();
}

QnEventLogDialog *ActionHandler::businessEventsLogDialog() const {
    return m_businessEventsLogDialog.data();
}

QnCameraListDialog *ActionHandler::cameraListDialog() const {
    return m_cameraListDialog.data();
}

QnCameraAdditionDialog *ActionHandler::cameraAdditionDialog() const {
    return m_cameraAdditionDialog.data();
}

QnSystemAdministrationDialog *ActionHandler::systemAdministrationDialog() const {
    return m_systemAdministrationDialog.data();
}

void ActionHandler::submitDelayedDrops()
{
    if (m_delayedDropGuard)
        return;

    if (!context()->user())
        return;

    if (!context()->workbench()->currentLayout()->resource())
        return;

    QScopedValueRollback<bool> guard(m_delayedDropGuard, true);

    QnResourceList resources;
    ec2::ApiLayoutTourDataList tours;

    for (const auto& data: m_delayedDrops)
    {
        MimeData mimeData = MimeData::deserialized(data, resourcePool());

        for (const auto& tour: layoutTourManager()->tours(mimeData.entities()))
            tours.push_back(tour);

        resources.append(mimeData.resources());
    }

    m_delayedDrops.clear();

    if (resources.empty() && tours.empty())
        return;

    resourcePool()->addNewResources(resources);

    workbench()->clear();
    if (!resources.empty())
        menu()->trigger(action::OpenInNewTabAction, resources);
    for (const auto& tour: tours)
        menu()->trigger(action::ReviewLayoutTourAction, {Qn::UuidRole, tour.id});
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void ActionHandler::at_context_userChanged(const QnUserResourcePtr &user) {
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
        for (const QnLayoutResourcePtr &layout : resourcePool()->getResourcesWithParentId(user->getId()).filtered<QnLayoutResource>())
        {
            if (layout->hasFlags(Qn::local) && !layout->isFile())
                resourcePool()->removeResource(layout);
        }
    }

    if (workbench()->layouts().empty())
        menu()->trigger(action::OpenNewTabAction);

    submitDelayedDrops();
}

void ActionHandler::at_workbench_cellSpacingChanged()
{
    qreal value = workbench()->currentLayout()->cellSpacing();

    if (qFuzzyIsNull(value))
        action(action::SetCurrentLayoutItemSpacingNoneAction)->setChecked(true);
    else if (qFuzzyCompare(QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Medium), value))
        action(action::SetCurrentLayoutItemSpacingMediumAction)->setChecked(true);
    else if (qFuzzyCompare(QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Large), value))
        action(action::SetCurrentLayoutItemSpacingLargeAction)->setChecked(true);
    else
        action(action::SetCurrentLayoutItemSpacingSmallAction)->setChecked(true); //default value
}

void ActionHandler::at_nextLayoutAction_triggered()
{
    if (action(action::ToggleLayoutTourModeAction)->isChecked())
        return;

    const auto total = workbench()->layouts().size();
    workbench()->setCurrentLayoutIndex((workbench()->currentLayoutIndex() + 1) % total);
}

void ActionHandler::at_previousLayoutAction_triggered()
{
    if (action(action::ToggleLayoutTourModeAction)->isChecked())
        return;

    const auto total = workbench()->layouts().size();
    workbench()->setCurrentLayoutIndex((workbench()->currentLayoutIndex() - 1 + total) % total);
}

void ActionHandler::showSingleCameraErrorMessage(const QString& explanation)
{
    static const auto kMessageText = tr("Failed to change password");
    QnMessageBox::critical(mainWindowWidget(), kMessageText, explanation);
}

void ActionHandler::showMultipleCamerasErrorMessage(
    int totalCameras,
    const QnVirtualCameraResourceList& camerasWithError,
    const QString& explanation)
{
    static const auto kMessageTemplate = tr("Failed to change password on %1 of %2 cameras");
    static const auto kSimpleOptions = QnResourceListView::Options(
        QnResourceListView::HideStatusOption
        | QnResourceListView::ServerAsHealthMonitorOption
        | QnResourceListView::SortAsInTreeOption);

    QnSessionAwareMessageBox messageBox(mainWindowWidget());
    messageBox.setIcon(QnMessageBoxIcon::Critical);
    messageBox.setText(kMessageTemplate.arg(
        QString::number(camerasWithError.size()), QString::number(totalCameras)));
    if (!explanation.isEmpty())
        messageBox.setInformativeText(explanation);
    messageBox.addCustomWidget(
        new QnResourceListView(camerasWithError, kSimpleOptions, &messageBox),
        QnMessageBox::Layout::AfterMainLabel);

    messageBox.setStandardButtons(QDialogButtonBox::Ok);
    messageBox.exec();
}

void ActionHandler::changeDefaultPasswords(
    const QString& previousPassword,
    const QnVirtualCameraResourceList& cameras,
    bool forceShowCamerasList)
{
    const auto server = commonModule()->currentServer();
    const auto serverConnection = server ? server->restConnection() : rest::QnConnectionPtr();
    if (!serverConnection)
    {
        NX_EXPECT(false, "No connection to server");
        return;
    }

    QnCameraPasswordChangeDialog dialog(
        previousPassword, cameras, forceShowCamerasList, mainWindowWidget());
    if (dialog.exec() != QDialog::Accepted)
        return;

    using PasswordChangeResult = QPair<QnVirtualCameraResourcePtr, QnRestResult>;
    using PassswordChangeResultList = QList<PasswordChangeResult>;

    const auto errorResultsStorage = QSharedPointer<PassswordChangeResultList>(
        new PassswordChangeResultList());

    const auto password = dialog.password();
    const auto guard = QPointer<ActionHandler>(this);
    const auto completionGuard = QnRaiiGuard::createDestructible(
        [this, guard, cameras, errorResultsStorage, password, forceShowCamerasList]()
        {
            if (!guard || errorResultsStorage->isEmpty())
                return;

            // Show error dialog and try one more time
            const auto camerasWithError =
                [errorResultsStorage]()
                {
                    QnVirtualCameraResourceList result;
                    for (const auto errorResult: *errorResultsStorage)
                        result.push_back(errorResult.first);
                    return result;
                }();

            auto explanation =
                [errorResultsStorage]()
                {
                    QnRestResult::Error error = errorResultsStorage->first().second.error;
                    if (errorResultsStorage->size() != 1)
                    {
                        const bool hasAnotherError = std::any_of(
                            errorResultsStorage->begin() + 1, errorResultsStorage->end(),
                            [error](const PasswordChangeResult& result)
                            {
                                return error != result.second.error;
                            });

                        if (hasAnotherError)
                            error = QnRestResult::NoError;
                    }

                    return error == QnRestResult::NoError
                        ? QString() //< NoError means different or unspecified error
                        : errorResultsStorage->first().second.errorString;
                }();

            if (cameras.size() > 1)
                showMultipleCamerasErrorMessage(cameras.size(), camerasWithError, explanation);
            else
                showSingleCameraErrorMessage(explanation);

            changeDefaultPasswords(password, camerasWithError, forceShowCamerasList);
        });

    for (const auto camera: cameras)
    {
        auto auth = camera->getDefaultAuth();
        auth.setPassword(password);

        const auto resultCallback =
            [guard, completionGuard, camera, errorResultsStorage]
                (bool success, rest::Handle /*handle*/, QnRestResult result)
            {
                if (!guard)
                    return;

                if (!success)
                    errorResultsStorage->append(PasswordChangeResult(camera, QnRestResult()));
                else if (result.error != QnRestResult::NoError)
                    errorResultsStorage->append(PasswordChangeResult(camera, result));
            };

        serverConnection->changeCameraPassword(
            camera->getId(), auth, resultCallback, QThread::currentThread());
    }
}

void ActionHandler::at_changeDefaultCameraPassword_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto camerasWithDefaultPassword =
        parameters.resources().filtered<QnVirtualCameraResource>();

    if (camerasWithDefaultPassword.isEmpty())
    {
        NX_EXPECT(false, "No cameras with default password");
        return;
    }

    const bool forceShowCamerasList = parameters.argument(Qn::ForceShowCamerasList, false);
    changeDefaultPasswords(QString(), camerasWithDefaultPassword, forceShowCamerasList);
}

void ActionHandler::at_openInLayoutAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    QnLayoutResourcePtr layout = parameters.argument<QnLayoutResourcePtr>(Qn::LayoutResourceRole);
    NX_ASSERT(layout, "No layout provided.");
    if (!layout)
        return;

    QPointF position = parameters.argument<QPointF>(Qn::ItemPositionRole);

    const int maxItems = qnRuntime->maxSceneItems();

    bool adjustAspectRatio = layout->getItems().isEmpty() || !layout->hasCellAspectRatio();

    QnResourceWidgetList widgets = parameters.widgets();
    if (!widgets.empty() && position.isNull() && layout->getItems().empty())
    {
        static const auto kExtraItemRoles =
            {Qn::ItemTimeRole, Qn::ItemPausedRole, Qn::ItemSpeedRole};

        using ExtraItemDataHash = QHash<Qn::ItemDataRole, QVariant>;
        QHash<QnUuid, ExtraItemDataHash> extraItemRoleValues;

        QHash<QnUuid, QnLayoutItemData> itemDataByUuid;
        for (auto widget: widgets)
        {
            QnLayoutItemData data = widget->item()->data();
            const auto oldUuid = data.uuid;
            data.uuid = QnUuid::createUuid();
            data.flags = Qn::PendingGeometryAdjustment;
            itemDataByUuid[oldUuid] = data;

            // Do not save position and state for cameras which footage we cannot see.
            if (const auto camera = widget->resource().dynamicCast<QnVirtualCameraResource>())
            {
                if (!accessController()->hasPermissions(camera, Qn::ViewFootagePermission))
                    continue;
            }

            for (const auto extraItemRole: kExtraItemRoles)
            {
                const auto value =
                    qnResourceRuntimeDataManager->layoutItemData(oldUuid, extraItemRole);
                if (value.isValid())
                    extraItemRoleValues[data.uuid].insert(extraItemRole, value);
            }
        }

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

            const auto values = extraItemRoleValues[data.uuid];
            for (auto it = values.begin(); it != values.end(); ++it)
                qnResourceRuntimeDataManager->setLayoutItemData(data.uuid, it.key(), it.value());
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

void ActionHandler::at_openInCurrentLayoutAction_triggered()
{
    auto parameters = menu()->currentParameters(sender());
    const auto currentLayout = workbench()->currentLayout();

    // Check if we are in videowall control mode
    QnUuid videoWallItemGuid = currentLayout->data(Qn::VideoWallItemGuidRole).value<QnUuid>();
    if (!videoWallItemGuid.isNull())
    {
        QnVideoWallItemIndex index = resourcePool()->getVideoWallItemByUuid(videoWallItemGuid);
        const auto resources = parameters.resources();

        // Displaying message delayed to avoid waiting cursor (see drop_instrument.cpp:245)
        if (!messages::Videowall::checkLocalFiles(mainWindowWidget(), index, resources, true))
            return;
    }

    parameters.setArgument(Qn::LayoutResourceRole, currentLayout->resource());
    menu()->trigger(action::OpenInLayoutAction, parameters);
}

void ActionHandler::at_openInNewTabAction_triggered()
{
    // Stop layout tour if it is running.
    if (action(action::ToggleLayoutTourModeAction)->isChecked())
        menu()->trigger(action::ToggleLayoutTourModeAction);

    auto parameters = menu()->currentParameters(sender());

    const auto layouts = parameters.resources().filtered<QnLayoutResource>();
    for (const auto& layout: layouts)
    {
        auto wbLayout = QnWorkbenchLayout::instance(layout);
        if (!wbLayout)
        {
            wbLayout = qnWorkbenchLayoutsFactory->create(layout, workbench());
            workbench()->addLayout(wbLayout);
        }
        /* Explicit set that we do not control videowall through this layout */
        wbLayout->setData(Qn::VideoWallItemGuidRole, qVariantFromValue(QnUuid()));
        workbench()->setCurrentLayout(wbLayout);
    }

    if (parameters.widgets().isEmpty())
    {
        // Called from resources tree view
        const auto openable = parameters.resources().filtered(QnResourceAccessFilter::isOpenableInLayout);
        if (openable.empty())
            return;

        parameters.setResources(openable);
        menu()->trigger(action::OpenNewTabAction);
    }
    else
    {
        // Action is called from the current layout context menu.

        const auto widgets = parameters.widgets();
        const bool hasViewFootagePermission = std::any_of(widgets.cbegin(), widgets.cend(),
            [this](const QnResourceWidget* widget)
            {
                const auto camera = widget->resource().dynamicCast<QnVirtualCameraResource>();
                return camera
                    && accessController()->hasPermissions(camera, Qn::ViewFootagePermission);
            });

        const auto streamSynchronizer = context()->instance<QnWorkbenchStreamSynchronizer>();
        const auto state = hasViewFootagePermission
            ? streamSynchronizer->state()
            : QnStreamSynchronizationState::live();

        action::Parameters params;
        params.setArgument(Qn::LayoutSyncStateRole, state);
        menu()->trigger(action::OpenNewTabAction, params);
    }

    menu()->trigger(action::DropResourcesAction, parameters);
}

void ActionHandler::at_openInNewWindowAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    QnResourceList filtered;
    for (const auto& resource: parameters.resources())
    {
        if (menu()->canTrigger(action::OpenInNewWindowAction, resource))
            filtered << resource;
    }
    if (filtered.isEmpty())
        return;

    openResourcesInNewWindow(filtered);
}

void ActionHandler::at_openCurrentLayoutInNewWindowAction_triggered()
{
    menu()->trigger(action::OpenInNewWindowAction, workbench()->currentLayout()->resource());
}

void ActionHandler::at_openNewWindowAction_triggered()
{
    openNewWindow(QStringList());
}

void ActionHandler::at_reviewLayoutTourInNewWindowAction_triggered()
{
    // For now place method here until openNewWindow code would be shared.
    const auto parameters = menu()->currentParameters(sender());
    auto id = parameters.argument<QnUuid>(Qn::UuidRole);

    MimeData data;
    data.setEntities({id});
    openNewWindow({lit("--delayed-drop"), data.serialized()});
}

void ActionHandler::at_cameraListChecked(int status, const QnCameraListReply& reply, int handle)
{
    if (!m_awaitingMoveCameras.contains(handle))
        return;
    QnVirtualCameraResourceList modifiedResources = m_awaitingMoveCameras.value(handle).cameras;
    QnMediaServerResourcePtr server = m_awaitingMoveCameras.value(handle).dstServer;
    m_awaitingMoveCameras.remove(handle);

    if (status != 0)
    {
        const auto text = QnDeviceDependentStrings::getNameFromSet(
            resourcePool(),
            QnCameraDeviceStringSet(
                tr("Failed to move %n devices", "", modifiedResources.size()),
                tr("Failed to move %n cameras", "", modifiedResources.size()),
                tr("Failed to move %n I/O Modules", "", modifiedResources.size())),
            modifiedResources);

        const auto extras = tr("Server \"%1\" is not responding.").arg(server->getName());
        QnMessageBox messageBox(QnMessageBoxIcon::Critical,
            text, extras, QDialogButtonBox::Ok, QDialogButtonBox::Ok,
            mainWindowWidget());

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
            resourcePool(),
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
            mainWindowWidget());

        messageBox.addButton(tr("Move"), QDialogButtonBox::YesRole, Qn::ButtonAccent::Standard);
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

void ActionHandler::at_convertCameraToEntropix_triggered()
{
    using nx::vms::common::core::resource::SensorDescription;
    using nx::vms::common::core::resource::CombinedSensorsDescription;

    const auto& parameters = menu()->currentParameters(sender());
    const auto& resources = parameters.resources();

    for (const auto& resource: resources)
    {
        const auto& camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (!camera)
            continue;

        if (camera->hasCombinedSensors())
        {
            camera->setCombinedSensorsDescription(CombinedSensorsDescription());
        }
        else
        {
            camera->setCombinedSensorsDescription(
                CombinedSensorsDescription{
                    SensorDescription(
                        QRectF(0, 0, 0.5, 1.0), SensorDescription::Type::blackAndWhite),
                    SensorDescription(
                        QRectF(0.5, 0, 0.5, 1.0))
                });
        }
        camera->saveParamsAsync();
    }
}

void ActionHandler::at_openNewScene_triggered()
{
    if (!m_mainWindow)
    {
        m_mainWindow = new experimental::MainWindow(
            qnClientCoreModule->mainQmlEngine(), context());
        m_mainWindow->resize(mainWindowWidget()->size());
    }

    m_mainWindow->show();
    m_mainWindow->raise();
}

void ActionHandler::at_moveCameraAction_triggered() {
    const auto parameters = menu()->currentParameters(sender());

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

void ActionHandler::at_dropResourcesAction_triggered()
{
    // Layout Tour Handler will process this action itself
    if (context()->workbench()->currentLayout()->isLayoutTourReview())
        return;

    auto parameters = menu()->currentParameters(sender());

    QnResourceList resources = parameters.resources();
    QnLayoutResourceList layouts = resources.filtered<QnLayoutResource>();
    foreach(QnLayoutResourcePtr r, layouts)
        resources.removeOne(r);

    QnVideoWallResourceList videowalls = resources.filtered<QnVideoWallResource>();
    foreach(QnVideoWallResourcePtr r, videowalls)
        resources.removeOne(r);

    if (!workbench()->currentLayout()->resource())
        menu()->trigger(action::OpenNewTabAction);

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
        if (parameters.widgets().isEmpty()) //< Triggered by resources tree view
            parameters.setResources(resources);
        if (!menu()->triggerIfPossible(action::OpenInCurrentLayoutAction, parameters))
            menu()->triggerIfPossible(action::OpenInNewTabAction, parameters);
    }

    if (!layouts.empty())
        menu()->trigger(action::OpenInNewTabAction, layouts);

    for (const auto& videoWall: videowalls)
        menu()->trigger(action::OpenVideoWallReviewAction, videoWall);
}

void ActionHandler::at_delayedDropResourcesAction_triggered()
{
    QByteArray data = menu()->currentParameters(sender()).argument<QByteArray>(
        Qn::SerializedDataRole);
    m_delayedDrops.push_back(data);

    submitDelayedDrops();
}

void ActionHandler::at_instantDropResourcesAction_triggered()
{
    QByteArray data = menu()->currentParameters(sender()).argument<QByteArray>(
        Qn::SerializedDataRole);
    MimeData mimeData = MimeData::deserialized(data, resourcePool());

    if (mimeData.resources().empty())
        return;

    resourcePool()->addNewResources(mimeData.resources());

    workbench()->clear();
    bool dropped = menu()->triggerIfPossible(action::OpenInNewTabAction, mimeData.resources());
    if (dropped)
        action(action::ResourcesModeAction)->setChecked(true);

    // Security check - just in case.
    if (workbench()->layouts().empty())
        menu()->trigger(action::OpenNewTabAction);
}

void ActionHandler::at_openFileAction_triggered() {
    QStringList filters;
    filters << tr("All Supported (*.nov *.avi *.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp *.jpg *.png *.gif *.bmp *.tiff)");
    filters << tr("Video (*.avi *.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp)");
    filters << tr("Pictures (*.jpg *.png *.gif *.bmp *.tiff)");
    filters << tr("All files (*.*)");

    QStringList files = QnFileDialog::getOpenFileNames(mainWindowWidget(),
        tr("Open File"),
        QString(),
        filters.join(lit(";;")),
        0,
        QnCustomFileDialog::fileDialogOptions());

    if (!files.isEmpty())
        menu()->trigger(action::DropResourcesAction, addToResourcePool(files));
}

void ActionHandler::at_openFolderAction_triggered() {
    QString dirName = QnFileDialog::getExistingDirectory(mainWindowWidget(),
        tr("Select folder..."),
        QString(),
        QnCustomFileDialog::directoryDialogOptions());

    if (!dirName.isEmpty())
        menu()->trigger(action::DropResourcesAction, addToResourcePool(dirName));
}

void ActionHandler::openFailoverPriorityDialog() {
    QScopedPointer<QnFailoverPriorityDialog> dialog(new QnFailoverPriorityDialog(mainWindowWidget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void ActionHandler::openBackupCamerasDialog() {
    QScopedPointer<QnBackupCamerasDialog> dialog(new QnBackupCamerasDialog(mainWindowWidget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void ActionHandler::openSystemAdministrationDialog(int page)
{
    QnNonModalDialogConstructor<QnSystemAdministrationDialog> dialogConstructor(
        m_systemAdministrationDialog, mainWindowWidget());
    systemAdministrationDialog()->setCurrentPage(page);
}

void ActionHandler::openLocalSettingsDialog(int page)
{
    QScopedPointer<QnLocalSettingsDialog> dialog(new QnLocalSettingsDialog(mainWindowWidget()));
    dialog->setCurrentPage(page);
    dialog->exec();
}

void ActionHandler::at_aboutAction_triggered() {
    QScopedPointer<QnAboutDialog> dialog(new QnAboutDialog(mainWindowWidget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void ActionHandler::at_businessEventsAction_triggered() {
    menu()->trigger(action::OpenBusinessRulesAction);
}

void ActionHandler::at_openBusinessRulesAction_triggered()
{
    QnNonModalDialogConstructor<QnBusinessRulesDialog> dialogConstructor(m_businessRulesDialog, mainWindowWidget());

    QStringList filter;
    const auto parameters = menu()->currentParameters(sender());
    QnVirtualCameraResourceList cameras = parameters.resources().filtered<QnVirtualCameraResource>();
    if (!cameras.isEmpty())
    {
        NX_ASSERT(cameras.size() == 1); // currently filter is not implemented for several cameras
        for (const auto& camera: cameras)
            filter.append(camera->getId().toSimpleString());
    }
    businessRulesDialog()->setFilter(filter.join(L' '));
}

void ActionHandler::at_webClientAction_triggered()
{
    const auto server = commonModule()->currentServer();
    if (!server)
        return;

#ifdef WEB_CLIENT_SUPPORTS_PROXY
#error Reimplement VMS-6586
    openInBrowser(server, kPath, kFragment);
#endif

    if (nx::network::isCloudServer(server))
    {
        menu()->trigger(action::OpenCloudViewSystemUrl);
    }
    else
    {
        static const auto kPath = lit("/static/index.html");
        static const auto kFragment = lit("/view");
        openInBrowserDirectly(server, kPath, kFragment);
    }
}

void ActionHandler::at_webAdminAction_triggered()
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

qint64 ActionHandler::getFirstBookmarkTimeMs()
{
    static const qint64 kOneDayOffsetMs = 60 * 60 * 24 * 1000;
    static const qint64 kOneYearOffsetMs = kOneDayOffsetMs * 365;
    const auto nowMs = qnSyncTime->currentMSecsSinceEpoch();
    const auto bookmarksWatcher = context()->instance<QnWorkbenchBookmarksWatcher>();
    const auto firstBookmarkUtcTimeMs = bookmarksWatcher->firstBookmarkUtcTimeMs();
    const bool firstTimeIsNotKnown = (firstBookmarkUtcTimeMs == QnWorkbenchBookmarksWatcher::kUndefinedTime);
    return (firstTimeIsNotKnown ? nowMs - kOneYearOffsetMs : firstBookmarkUtcTimeMs);
}

void ActionHandler::renameLocalFile(const QnResourcePtr& resource, const QString& newName)
{
    auto newFileName = newName;
    if (nx::utils::AppInfo::isWindows())
    {
        while (newFileName.endsWith(QChar(L' ')) || newFileName.endsWith(QChar(L'.')))
            newFileName.resize(newFileName.size() - 1);

        const QSet<QString> kReservedFilenames{
            lit("CON"), lit("PRN"), lit("AUX"), lit("NUL"),
            lit("COM1"), lit("COM2"), lit("COM3"), lit("COM4"),
            lit("COM5"), lit("COM6"), lit("COM7"), lit("COM8"), lit("COM9"),
            lit("LPT1"), lit("LPT2"), lit("LPT3"), lit("LPT4"), lit("LPT5"),
            lit("LPT6"), lit("LPT7"), lit("LPT8"), lit("LPT9") };

        const auto upperCaseName = newFileName.toUpper();
        if (kReservedFilenames.contains(upperCaseName))
        {
            messages::LocalFiles::reservedFilename(mainWindowWidget(), upperCaseName);
            return;
        }
    }

    static const QString kForbiddenChars =
        nx::utils::AppInfo::isWindows() ? lit("\\|/:*?\"<>")
      : nx::utils::AppInfo::isLinux()   ? lit("/")
      : nx::utils::AppInfo::isMacOsX()  ? lit("/:")
      : QString();

    for (const auto character: newFileName)
    {
        if (kForbiddenChars.contains(character))
        {
            messages::LocalFiles::invalidChars(mainWindowWidget(), kForbiddenChars);
            return;
        }
    }

    QFileInfo fi(resource->getUrl());
    QString filePath = fi.absolutePath() + QDir::separator() + newFileName;

    if (QFileInfo::exists(filePath))
    {
        messages::LocalFiles::fileExists(mainWindowWidget(), filePath);
        return;
    }

    QString targetPath = QFileInfo(filePath).absolutePath();
    if (!QDir().mkpath(targetPath))
    {
        messages::LocalFiles::pathInvalid(mainWindowWidget(), targetPath);
        return;
    }

    QFile writabilityTest(filePath);
    if (!writabilityTest.open(QIODevice::WriteOnly))
    {
        messages::LocalFiles::fileCannotBeWritten(mainWindowWidget(), filePath);
        return;
    }

    writabilityTest.remove();

    // If video is opened right now, it is quite hard to close it synchronously.
    static const int kTimeToCloseResourceMs = 300;

    resourcePool()->removeResource(resource);
    executeDelayedParented(
        [this, resource, filePath]()
        {
            QnResourcePtr result = resource;
            const auto oldPath = resource->getUrl();

            if (!QFile::rename(oldPath, filePath))
            {
                messages::LocalFiles::fileIsBusy(mainWindowWidget(), oldPath);
            }
            // Renamed file is discovered immediately by ResourceDirectoryBrowser.
            // It will deal with adding a resource with new name
        },
        kTimeToCloseResourceMs,
        this);
}

void ActionHandler::at_openBookmarksSearchAction_triggered()
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
        return new QnSearchBookmarksDialog(filterText, startTimeMs, endTimeMs, mainWindowWidget());
    };

    const bool firstTime = m_searchBookmarksDialog.isNull();
    const QnNonModalDialogConstructor<QnSearchBookmarksDialog> creator(m_searchBookmarksDialog, dialogCreationFunction);

    if (!firstTime)
        m_searchBookmarksDialog->setParameters(startTimeMs, endTimeMs, filterText);
}

void ActionHandler::at_openBusinessLogAction_triggered() {
    QnNonModalDialogConstructor<QnEventLogDialog> dialogConstructor(m_businessEventsLogDialog, mainWindowWidget());

    const auto parameters = menu()->currentParameters(sender());

    vms::event::EventType eventType = parameters.argument(Qn::EventTypeRole, vms::event::anyEvent);
    auto cameras = parameters.resources().filtered<QnVirtualCameraResource>();
    QSet<QnUuid> ids;
    for (auto camera: cameras)
        ids << camera->getId();

    // show diagnostics if Issues action was triggered
    if (eventType != vms::event::anyEvent || !ids.isEmpty())
    {
        businessEventsLogDialog()->disableUpdateData();
        businessEventsLogDialog()->setEventType(eventType);
        businessEventsLogDialog()->setActionType(vms::event::diagnosticsAction);
        auto now = QDateTime::currentMSecsSinceEpoch();
        businessEventsLogDialog()->setDateRange(now, now);
        businessEventsLogDialog()->setCameraList(ids);
        businessEventsLogDialog()->enableUpdateData();
    }
}

void ActionHandler::at_openAuditLogAction_triggered() {
    QnNonModalDialogConstructor<QnAuditLogDialog> dialogConstructor(m_auditLogDialog, mainWindowWidget());
    const auto parameters = menu()->currentParameters(sender());
}

void ActionHandler::at_cameraListAction_triggered() {
    QnNonModalDialogConstructor<QnCameraListDialog> dialogConstructor(m_cameraListDialog, mainWindowWidget());
    const auto parameters = menu()->currentParameters(sender());
    QnMediaServerResourcePtr server;
    if (!parameters.resources().isEmpty())
        server = parameters.resource().dynamicCast<QnMediaServerResource>();
    cameraListDialog()->setServer(server);
}

void ActionHandler::at_thumbnailsSearchAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    QnResourcePtr resource = parameters.resource();
    if (!resource)
        return;

    auto widget = parameters.widget();
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
    if (!periods.empty())
    {
        QnTimePeriodList localPeriods = periods.intersected(period);
        NX_ASSERT(!localPeriods.empty());

        if (!localPeriods.empty())
        {
            // Check if user selected period before the first chunk.
            const qint64 startDelta = localPeriods.begin()->startTimeMs - period.startTimeMs;
            if (startDelta > 0)
            {
                period.startTimeMs += startDelta;
                period.durationMs -= startDelta;
            }

            // Check if user selected period after the last chunk.
            const qint64 endDelta = period.endTimeMs() - localPeriods.last().endTimeMs();
            if (endDelta > 0)
            {
                period.durationMs -= endDelta;
            }
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
        QnMessageBox::warning(mainWindowWidget(),
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
        : Geometry::aspectRatio(viewportGeometry);

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
        if (resource->hasFlags(Qn::local_media))
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

    /**
     * TODO: #GDM #3.2
     * Adding layout to the resource pool is not needed, moreover it requires to add a lot of
     * additional checks in different places. But to remove this line we need to implement a
     * mechanism to cleanup QnResourceRuntimeDataManager layout item data.
    */
    resourcePool()->addResource(layout);
    menu()->trigger(action::OpenInNewTabAction, layout);
}

void ActionHandler::at_mediaFileSettingsAction_triggered() {
    QnResourcePtr resource = menu()->currentParameters(sender()).resource();
    if (!resource)
        return;

    QnMediaResourcePtr media = resource.dynamicCast<QnMediaResource>();
    if (!media)
        return;

    QScopedPointer<MediaFileSettingsDialog> dialog;
    if (resource->hasFlags(Qn::remote))
        dialog.reset(new QnSessionAware<MediaFileSettingsDialog>(mainWindowWidget()));
    else
        dialog.reset(new MediaFileSettingsDialog(mainWindowWidget()));

    dialog->updateFromResource(media);
    if (dialog->exec())
    {
        dialog->submitToResource(media);
    }
    else
    {
        auto centralWidget = display()->widget(Qn::CentralRole);
        if (auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(centralWidget))
            mediaWidget->setDewarpingParams(media->getDewarpingParams());
    }
}

void ActionHandler::at_cameraIssuesAction_triggered()
{
    menu()->trigger(action::OpenBusinessLogAction,
        menu()->currentParameters(sender())
        .withArgument(Qn::EventTypeRole, vms::event::anyCameraEvent));
}

void ActionHandler::at_cameraBusinessRulesAction_triggered() {
    menu()->trigger(action::OpenBusinessRulesAction,
        menu()->currentParameters(sender()));
}

void ActionHandler::at_cameraDiagnosticsAction_triggered() {
    QnVirtualCameraResourcePtr resource = menu()->currentParameters(sender()).resource().dynamicCast<QnVirtualCameraResource>();
    if (!resource)
        return;

    QScopedPointer<QnCameraDiagnosticsDialog> dialog(new QnCameraDiagnosticsDialog(mainWindowWidget()));
    dialog->setResource(resource);
    dialog->restart();
    dialog->exec();
}

void ActionHandler::at_serverAddCameraManuallyAction_triggered()
{
    const auto params = menu()->currentParameters(sender());
    const auto server = params.resource().dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    QnNonModalDialogConstructor<QnCameraAdditionDialog> dialogConstructor(m_cameraAdditionDialog, mainWindowWidget());

    QnCameraAdditionDialog* dialog = cameraAdditionDialog();

    if (dialog->server() != server) {
        if (dialog->state() == QnCameraAdditionDialog::Searching
            || dialog->state() == QnCameraAdditionDialog::Adding) {

            const auto result = QnMessageBox::question(mainWindowWidget(),
                tr("Cancel device adding?"), QString(),
                QDialogButtonBox::Yes | QDialogButtonBox::No,
                QDialogButtonBox::No);

            if (result != QDialogButtonBox::Yes)
                return;
        }
        dialog->setServer(server);
    }
}


void ActionHandler::at_addDeviceManually_triggered()
{
    const auto params = menu()->currentParameters(sender());
    const auto server = params.resource().dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    QnNonModalDialogConstructor<DeviceAdditionDialog> dialogContructor(
        m_deviceAdditionDialog, mainWindowWidget());

    m_deviceAdditionDialog->setServer(server);
}

void ActionHandler::at_serverLogsAction_triggered()
{
    QnMediaServerResourcePtr server = menu()->currentParameters(sender()).resource().dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    openInBrowser(server, lit("/api/showLog?lines=1000"));
}

void ActionHandler::at_serverIssuesAction_triggered()
{
    menu()->trigger(action::OpenBusinessLogAction,
        {Qn::EventTypeRole, vms::event::anyServerEvent});
}

void ActionHandler::at_pingAction_triggered()
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

void ActionHandler::at_openInFolderAction_triggered() {
    QnResourcePtr resource = menu()->currentParameters(sender()).resource();
    if (resource.isNull())
        return;

    QnEnvironment::showInGraphicalShell(resource->getUrl());
}

void ActionHandler::at_deleteFromDiskAction_triggered()
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
        mainWindowWidget());

    messageBox.addCustomWidget(new QnResourceListView(resources, &messageBox));
    auto result = messageBox.exec();
    if (result != QDialogButtonBox::Yes)
        return;

    QnFileProcessor::deleteLocalResources(resources);
}

bool ActionHandler::validateResourceName(const QnResourcePtr& resource, const QString& newName) const
{
    /*
     * Users, videowalls and local files should be checked to avoid names collisions.
     * Layouts are checked separately, servers and cameras can have the same name.
     */
    auto checkedFlags = resource->flags() & (Qn::user | Qn::videowall | Qn::local_media);
    if (!checkedFlags)
        return true;

    /* Resource cannot have both of these flags at once. */
    NX_ASSERT(checkedFlags == Qn::user
        || checkedFlags == Qn::videowall
        || checkedFlags == Qn::local_media);

    for (const auto& resource: resourcePool()->getResources())
    {
        if (!resource->hasFlags(checkedFlags))
            continue;

        if (resource->getName().compare(newName, Qt::CaseInsensitive) != 0)
            continue;

        if (checkedFlags == Qn::user)
        {
            QnMessageBox::warning(mainWindowWidget(), tr("There is another user with the same name"));
        }
        else if (checkedFlags == Qn::videowall)
        {
            messages::Videowall::anotherVideoWall(mainWindowWidget());
        }
        else if (checkedFlags == Qn::local_media)
        {
            // Silently skip. Should never get here really, leaving branch for safety.
        }

        return false;
    }

    return true;
}

void ActionHandler::at_renameAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

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

    // TODO: #vkutin #gdm Is the following block of code still in use?
    if (name.isEmpty())
    {
        bool ok = false;
        do
        {
            name = QnInputDialog::getText(mainWindowWidget(),
                tr("Rename"),
                tr("Enter new name for the selected item:"),
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                QLineEdit::Normal,
                QString(),
                oldName).trimmed();

            if (name.isEmpty() || name == oldName)
                return;

        } while (!validateResourceName(resource, name));
    }

    if (name == oldName)
        return;

    if (QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>())
    {
        if (layout->isFile())
            renameLocalFile(layout, name);
        else
            context()->instance<LayoutsHandler>()->renameLayout(layout, name);
    }
    else if (nodeType == Qn::RecorderNode)
    {
        /* Recorder name should not be validated. */
        QString groupId = camera->getGroupId();

        QnVirtualCameraResourceList modified = resourcePool()->getResources().filtered<QnVirtualCameraResource>([groupId](const QnVirtualCameraResourcePtr &camera)
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
    else if (auto videowall = resource.dynamicCast<QnVideoWallResource>())
    {
        qnResourcesChangesManager->saveVideoWall(videowall,
            [name](const QnVideoWallResourcePtr& videowall)
            {
                videowall->setName(name);
            });
    }
    else if (auto webPage = resource.dynamicCast<QnWebPageResource>())
    {
        qnResourcesChangesManager->saveWebPage(webPage,
            [name](const QnWebPageResourcePtr& webPage)
            {
                webPage->setName(name);
            });
    }
    else if (auto archive = resource.dynamicCast<QnAbstractArchiveResource>())
    {
        renameLocalFile(archive, name);
    }
    else
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Invalid resource type to rename");
    }
}

void ActionHandler::at_removeFromServerAction_triggered()
{
    QnResourceList resources = menu()->currentParameters(sender()).resources();

    /* Layouts will be removed in their own handler. Also separately check each resource. */
    resources = resources.filtered(
        [this](const QnResourcePtr& resource)
        {
            return menu()->canTrigger(action::RemoveFromServerAction, resource)
                && !resource->hasFlags(Qn::layout);
        });

    if (ui::messages::Resources::deleteResources(mainWindowWidget(), resources))
        qnResourcesChangesManager->deleteResources(resources);
}

void ActionHandler::closeApplication(bool force) {
    /* Try close, if force - exit anyway. */
    if (!context()->instance<QnWorkbenchStateManager>()->tryClose(force) && !force)
        return;

    menu()->trigger(action::BeforeExitAction);
    context()->setClosingDown(true);
    qApp->exit(0);
    applauncher::scheduleProcessKill(QCoreApplication::applicationPid(), PROCESS_TERMINATE_TIMEOUT);
}

void ActionHandler::at_beforeExitAction_triggered() {
    deleteDialogs();
}


QnAdjustVideoDialog* ActionHandler::adjustVideoDialog() {
    return m_adjustVideoDialog.data();
}

void ActionHandler::at_adjustVideoAction_triggered()
{
    auto widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if (!widget)
        return;

    QnNonModalDialogConstructor<QnAdjustVideoDialog> dialogConstructor(m_adjustVideoDialog, mainWindowWidget());
    adjustVideoDialog()->setWidget(widget);
}

void ActionHandler::at_createZoomWindowAction_triggered() {
    const auto params = menu()->currentParameters(sender());

    auto widget = params.widget<QnMediaResourceWidget>();
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
    addParams.zoomWindowRectangleVisible =
        params.argument<bool>(Qn::ItemZoomWindowRectangleVisibleRole);
    addParams.rotation = widget->item()->rotation();

    addToLayout(workbench()->currentLayout()->resource(), widget->resource()->toResourcePtr(), addParams);
}

void ActionHandler::at_setAsBackgroundAction_triggered() {

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

    QnProgressDialog *progressDialog = new QnProgressDialog(mainWindowWidget());
    progressDialog->setWindowTitle(tr("Updating Background..."));
    progressDialog->setLabelText(tr("Image processing may take a few moments. Please be patient."));
    progressDialog->setRange(0, 0);
    progressDialog->setCancelButton(NULL);
    connect(progressDialog, &QnProgressDialog::canceled, progressDialog, &QObject::deleteLater);

    const auto parameters = menu()->currentParameters(sender());

    ServerImageCache *cache = new ServerImageCache(this);
    cache->setProperty(uploadingImageARPropertyName, parameters.widget()->aspectRatio());
    connect(cache, &ServerImageCache::fileUploaded, progressDialog, &QObject::deleteLater);
    connect(cache, &ServerImageCache::fileUploaded, cache, &QObject::deleteLater);
    connect(cache, &ServerImageCache::fileUploaded, this, [this, checkCondition](const QString &filename, ServerFileCache::OperationResult status) {
        if (!checkCondition())
            return;

        if (status == ServerFileCache::OperationResult::sizeLimitExceeded)
        {
            // TODO: #GDM #3.1 move out strings and logic to separate class (string.h:bytesToString)
            //Important: maximumFileSize() is hardcoded in 1024-base
            const auto maxFileSize = ServerFileCache::maximumFileSize() / (1024 * 1024);
            QnMessageBox::warning(mainWindowWidget(),
                tr("Image too big"),
                tr("Maximum size is %1 MB.").arg(maxFileSize));
            return;
        }

        if (status != ServerFileCache::OperationResult::ok)
        {
            QnMessageBox::critical(mainWindowWidget(), tr("Failed to upload image"));
            return;
        }

        setCurrentLayoutBackground(filename);

    });

    cache->storeImage(parameters.resource()->getUrl());
    progressDialog->exec();
}

void ActionHandler::setCurrentLayoutBackground(const QString &filename) {
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

void ActionHandler::at_scheduleWatcher_scheduleEnabledChanged() {
    // TODO: #Elric totally evil copypasta and hacky workaround.
    bool enabled =
        context()->instance<QnWorkbenchScheduleWatcher>()->isScheduleEnabled() &&
        accessController()->hasGlobalPermission(Qn::GlobalAdminPermission);

    action(action::TogglePanicModeAction)->setEnabled(enabled);
    if (!enabled)
        action(action::TogglePanicModeAction)->setChecked(false);
}

void ActionHandler::at_togglePanicModeAction_toggled(bool checked) {
    QnMediaServerResourceList resources = resourcePool()->getAllServers(Qn::AnyStatus);

    foreach(QnMediaServerResourcePtr resource, resources)
    {
        bool isPanicMode = resource->getPanicMode() != Qn::PM_None;
        if (isPanicMode != checked) {
            Qn::PanicMode val = Qn::PM_None;
            if (checked)
                val = Qn::PM_User;
            resource->setPanicMode(val);
            propertyDictionary()->saveParamsAsync(resource->getId());
        }
    }
}

void ActionHandler::at_workbench_itemChanged(Qn::ItemRole role)
{
    if (role == Qn::CentralRole && adjustVideoDialog() && adjustVideoDialog()->isVisible())
    {
        QnWorkbenchItem *item = context()->workbench()->item(Qn::CentralRole);
        QnMediaResourceWidget* widget = dynamic_cast<QnMediaResourceWidget*> (context()->display()->widget(item));
        if (widget)
            adjustVideoDialog()->setWidget(widget);
    }
}

void ActionHandler::at_whatsThisAction_triggered()
{
    if (QWhatsThis::inWhatsThisMode())
        QWhatsThis::leaveWhatsThisMode();
    else
        QWhatsThis::enterWhatsThisMode();
}

void ActionHandler::at_escapeHotkeyAction_triggered()
{
    if (QWhatsThis::inWhatsThisMode())
        QWhatsThis::leaveWhatsThisMode();
}

void ActionHandler::at_browseUrlAction_triggered() {
    QString url = menu()->currentParameters(sender()).argument<QString>(Qn::UrlRole);
    if (url.isEmpty())
        return;

    QDesktopServices::openUrl(nx::utils::Url::fromUserInput(url).toQUrl());
}

void ActionHandler::at_versionMismatchMessageAction_triggered()
{
    if (commonModule()->isReadOnly())
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
        new QnSessionAwareMessageBox(mainWindowWidget()));
    messageBox->setIcon(QnMessageBoxIcon::Warning);
    messageBox->setText(tr("Components of System have different versions:"));
    messageBox->setInformativeText(extras);
    messageBox->setCheckBoxEnabled();

    const auto updateButton = messageBox->addButton(
        tr("Update..."), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
    messageBox->addButton(
        tr("Skip"), QDialogButtonBox::RejectRole);

    messageBox->exec();

    if (messageBox->isChecked())
        qnClientShowOnce->setFlag(kVersionMismatchShowOnceKey);

    if (messageBox->clickedButton() == updateButton)
        menu()->trigger(action::SystemUpdateAction);
}

void ActionHandler::at_betaVersionMessageAction_triggered()
{
    if (context()->closingDown())
        return;

    if (qnClientShowOnce->testFlag(kBetaVersionShowOnceKey))
        return;

    QnMessageBox dialog(QnMessageBoxIcon::Information,
        tr("Beta version %1").arg(QnAppInfo::applicationVersion()),
        tr("Some functionality may be unavailable or not working properly."),
        QDialogButtonBox::Ok, QDialogButtonBox::Ok, mainWindowWidget());

    dialog.setCheckBoxEnabled();
    dialog.exec();

    if (dialog.isChecked())
        qnClientShowOnce->setFlag(kBetaVersionShowOnceKey);
}

void ActionHandler::checkIfStatisticsReportAllowed() {

    const QnMediaServerResourceList servers = resourcePool()->getResources<QnMediaServerResource>();

    /* Check if we are not connected yet. */
    if (servers.isEmpty())
        return;

    /* Check if user has already made a decision. */
    if (qnGlobalSettings->isStatisticsAllowedDefined())
        return;

    /* User cannot disable statistics collecting, so don't make him sorrow. */
    if (commonModule()->isReadOnly())
        return;

    /* Suppress notification if no server has internet access. */
    bool atLeastOneServerHasInternetAccess = boost::algorithm::any_of(servers, [](const QnMediaServerResourcePtr &server) {
        return (server->getServerFlags() & Qn::SF_HasPublicIP);
    });
    if (!atLeastOneServerHasInternetAccess)
        return;

    QnMessageBox::information(mainWindowWidget(),
        tr("System sends anonymous usage statistics"),
        tr("It will be used by software development team to improve your user experience.")
            + L'\n' + tr("To disable it, go to System Administration dialog."));

    qnGlobalSettings->setStatisticsAllowed(true);
    qnGlobalSettings->synchronizeNow();
}

void ActionHandler::at_queueAppRestartAction_triggered()
{

    auto tryToRestartClient = []
        {
            using namespace applauncher;

            /* Try to run applauncher if it is not running. */
            if (!checkOnline())
                return false;

            const auto result = restartClient();
            if (result == applauncher::api::ResultType::Value::ok)
                return true;

            static const int kMaxTries = 5;
            for (int i = 0; i < kMaxTries; ++i)
            {
                QThread::msleep(100);
                qApp->processEvents();
                if (restartClient() == applauncher::api::ResultType::ok)
                    return true;
            }
            return false;
        };

    if (!tryToRestartClient())
    {
        QnConnectionDiagnosticsHelper::failedRestartClientMessage(mainWindowWidget());
        return;
    }
    menu()->trigger(action::DelayedForcedExitAction);
}

void ActionHandler::at_selectTimeServerAction_triggered()
{
    openSystemAdministrationDialog(QnSystemAdministrationDialog::TimeServerSelection);
}

void ActionHandler::openInBrowserDirectly(const QnMediaServerResourcePtr& server,
    const QString& path, const QString& fragment)
{
    // TODO: #akolesnikov #3.1 VMS-2806 deprecate this method, always use openInBrowser()

    if (nx::network::isCloudServer(server))
        return;

    nx::utils::Url url(server->getApiUrl());
    url.setUserName(QString());
    url.setPassword(QString());
    url.setScheme(lit("http"));
    url.setPath(path);
    url.setFragment(fragment);
    url = qnClientModule->networkProxyFactory()->urlToResource(url, server, lit("proxy"));
    QDesktopServices::openUrl(url.toQUrl());
}

void ActionHandler::openInBrowser(const QnMediaServerResourcePtr& server,
    const QString& path, const QString& fragment)
{
    if (!server || !context()->user())
        return;
    // path may contains path + url query params
    // TODO: #akolesnikov #3.1 VMS-2806
    nx::utils::Url serverUrl(server->getApiUrl().toString() + path);
    serverUrl.setFragment(fragment);

    nx::utils::Url proxyUrl = qnClientModule->networkProxyFactory()->urlToResource(serverUrl, server);
    proxyUrl.setPath(lit("/api/getNonce"));

    if (m_serverRequests.find(proxyUrl) == m_serverRequests.end())
    {
        // No other requests to this proxy, so we have to get nonce by ourselves.
        auto reply = new QnAsyncHttpClientReply(nx_http::AsyncHttpClient::create(), this);
        connect(
            reply, &QnAsyncHttpClientReply::finished,
            this, &ActionHandler::at_nonceReceived);

        // TODO: Think of preloader in case of user complains about delay.
        reply->asyncHttpClient()->doGet(proxyUrl);
    }

    m_serverRequests.emplace(proxyUrl, ServerRequest{server, serverUrl});
}

void ActionHandler::at_nonceReceived(QnAsyncHttpClientReply *reply)
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
        QnMessageBox::critical(mainWindowWidget(), tr("Failed to open server web page"));
        return;
    }

    for (const auto& request: requests)
    {
        const auto appserverUrl = commonModule()->currentUrl();
        const auto authParam = createHttpQueryAuthParam(
            appserverUrl.userName(), appserverUrl.password(),
            auth.realm, nx_http::Method::get, auth.nonce.toUtf8());

        nx::utils::Url targetUrl(request.url);
        QUrlQuery urlQuery(targetUrl.toQUrl());
        urlQuery.addQueryItem(lit("auth"), QLatin1String(authParam));
        targetUrl.setQuery(urlQuery);

        targetUrl = qnClientModule->networkProxyFactory()->urlToResource(targetUrl, request.server);

        auto gateway = nx::cloud::gateway::VmsGatewayEmbeddable::instance();
        targetUrl = nx::utils::Url(lit("http://%1/%2:%3:%4%5?%6")
            .arg(gateway->endpoint().toString()).arg(targetUrl.scheme())
            .arg(targetUrl.host()).arg(targetUrl.port())
            .arg(targetUrl.path()).arg(targetUrl.query()));

        QDesktopServices::openUrl(targetUrl.toQUrl());
    }
}

void ActionHandler::deleteDialogs() {
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

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
