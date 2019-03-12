#include "workbench_resources_settings_handler.h"

#include <QtWidgets/QAction>

#include <nx/vms/client/desktop/ini.h>
#include <common/common_module.h>
#include <client/client_settings.h>
#include <core/misc/schedule_task.h>
#include <core/resource/camera_resource.h>
#include <core/resource/fake_media_server.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/dialogs/resource_properties/user_roles_dialog.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/camera/camera_bitrate_calculator.h>

#include <nx/vms/client/desktop/resource_properties/layout/layout_settings_dialog.h>
#include <nx/vms/client/desktop/resource_properties/layout/redux/layout_settings_dialog_state_reducer.h>
#include <nx/vms/client/desktop/resource_properties/camera/export_schedule_resource_selection_dialog_delegate.h>
#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_dialog.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/utils/parameter_helper.h>
#include <nx/vms/client/desktop/debug_utils/utils/debug_custom_actions.h>

#include <nx/utils/scope_guard.h>

using namespace nx::vms::client::desktop;
using namespace ui;

QnWorkbenchResourcesSettingsHandler::QnWorkbenchResourcesSettingsHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(action::CameraSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_cameraSettingsAction_triggered);
    connect(action(action::ServerSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_serverSettingsAction_triggered);
    connect(action(action::NewUserAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_newUserAction_triggered);
    connect(action(action::UserSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_userSettingsAction_triggered);
    connect(action(action::UserRolesAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_userRolesAction_triggered);
    connect(action(action::LayoutSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_layoutSettingsAction_triggered);
    connect(action(action::CurrentLayoutSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_currentLayoutSettingsAction_triggered);
    connect(action(action::CopyRecordingScheduleAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_copyRecordingScheduleAction_triggered);

    connect(action(action::UpdateLocalFilesAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_updateLocalFilesAction_triggered);

    registerDebugAction(
        "Tracing: Layout settings",
        [] { LayoutSettingsDialogStateReducer::setTracingEnabled(true); });
}

QnWorkbenchResourcesSettingsHandler::~QnWorkbenchResourcesSettingsHandler()
{
}

void QnWorkbenchResourcesSettingsHandler::at_cameraSettingsAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    auto cameras = parameters.resources().filtered<QnVirtualCameraResource>();
    if (cameras.isEmpty())
        return;

    const auto parent = utils::extractParentWidget(parameters, mainWindowWidget());

    QnNonModalDialogConstructor<CameraSettingsDialog> dialogConstructor(
        m_cameraSettingsDialog, parent);
    dialogConstructor.disableAutoFocus();

    if (!m_cameraSettingsDialog->setCameras(cameras))
        return;

    if (parameters.hasArgument(Qn::FocusTabRole))
    {
        const auto tab = parameters.argument(Qn::FocusTabRole).toInt();
        m_cameraSettingsDialog->setCurrentPage(tab);
    }
}

void QnWorkbenchResourcesSettingsHandler::at_serverSettingsAction_triggered()
{
    const auto params = menu()->currentParameters(sender());
    QnMediaServerResourceList servers = params.resources().filtered<QnMediaServerResource>(
        [](const QnMediaServerResourcePtr &server)
        {
            return !server.dynamicCast<QnFakeMediaServerResource>();
        });

    NX_ASSERT(servers.size() == 1, "Invalid action condition");
    if(servers.isEmpty())
        return;

    QnMediaServerResourcePtr server = servers.first();

    bool hasAccess = accessController()->hasGlobalPermission(GlobalPermission::admin);
    NX_ASSERT(hasAccess, "Invalid action condition"); /*< It must be checked on action level. */
    if (!hasAccess)
        return;

    const auto parent = utils::extractParentWidget(params, mainWindowWidget());
    QnNonModalDialogConstructor<QnServerSettingsDialog> dialogConstructor(
        m_serverSettingsDialog, parent);

    dialogConstructor.disableAutoFocus();

    m_serverSettingsDialog->setServer(server);
    if (params.hasArgument(Qn::FocusTabRole))
        m_serverSettingsDialog->setCurrentPage(params.argument<int>(Qn::FocusTabRole), true);
}

void QnWorkbenchResourcesSettingsHandler::at_newUserAction_triggered()
{
    QnUserResourcePtr user(new QnUserResource(QnUserType::Local));
    user->setRawPermissions(GlobalPermission::liveViewerPermissions);

    // Shows New User dialog as modal because we can't pick anothr user from resources tree anyway.
    const auto params = menu()->currentParameters(sender());
    const auto parent = utils::extractParentWidget(params, mainWindowWidget());

    if (!m_userSettingsDialog)
        m_userSettingsDialog = new QnUserSettingsDialog(parent);
    else
        DialogUtils::setDialogParent(m_userSettingsDialog, parent);

    m_userSettingsDialog->setUser(user);
    m_userSettingsDialog->setCurrentPage(QnUserSettingsDialog::SettingsPage);
    m_userSettingsDialog->forcedUpdate();

    m_userSettingsDialog->exec();
}

void QnWorkbenchResourcesSettingsHandler::at_userSettingsAction_triggered()
{
    const auto params = menu()->currentParameters(sender());
    QnUserResourcePtr user = params.resource().dynamicCast<QnUserResource>();
    if (!user)
        return;

    bool hasAccess = accessController()->hasPermissions(user, Qn::ReadPermission);
    NX_ASSERT(hasAccess, "Invalid action condition");
    if (!hasAccess)
        return;

    const auto parent = utils::extractParentWidget(params, mainWindowWidget());
    QnNonModalDialogConstructor<QnUserSettingsDialog> dialogConstructor(
        m_userSettingsDialog, parent);

    // Navigating resource tree, we should not take focus. From System Administration - we must.
    bool force = params.argument(Qn::ForceRole, false);
    if (!force)
        dialogConstructor.disableAutoFocus();

    m_userSettingsDialog->setUser(user);
    if (params.hasArgument(Qn::FocusTabRole))
        m_userSettingsDialog->setCurrentPage(params.argument<int>(Qn::FocusTabRole), true);
    m_userSettingsDialog->forcedUpdate();
}

void QnWorkbenchResourcesSettingsHandler::at_userRolesAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto parent = utils::extractParentWidget(parameters, mainWindowWidget());

    QnUuid userRoleId = parameters.argument(Qn::UuidRole).value<QnUuid>();

    QScopedPointer<QnUserRolesDialog> dialog(new QnUserRolesDialog(parent));
    if (!userRoleId.isNull())
        dialog->selectUserRole(userRoleId);

    dialog->exec();
}

void QnWorkbenchResourcesSettingsHandler::at_layoutSettingsAction_triggered()
{
    const auto params = menu()->currentParameters(sender());
    openLayoutSettingsDialog(params.resource().dynamicCast<QnLayoutResource>());
}

void QnWorkbenchResourcesSettingsHandler::at_currentLayoutSettingsAction_triggered()
{
    openLayoutSettingsDialog(workbench()->currentLayout()->resource());
}

void QnWorkbenchResourcesSettingsHandler::at_copyRecordingScheduleAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    auto camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    const auto parent = utils::extractParentWidget(parameters, mainWindowWidget());

    bool motionUsed = false;
    bool dualStreamingUsed = false;

    const bool recordingEnabled = camera->isLicenseUsed();
    const auto schedule = camera->getScheduleTasks();

    if (recordingEnabled)
    {
        for (const auto& task: schedule)
        {
            switch (task.recordingType)
            {
                case Qn::RecordingType::motionAndLow:
                    dualStreamingUsed = true;
                    [[fallthrough]];
                case Qn::RecordingType::motionOnly:
                    motionUsed = true;
                    break;

                default:
                    break;
            }

            if (dualStreamingUsed)
                break;
        }
    }

    const bool hasVideo = camera->hasVideo();

    QScopedPointer<QnResourceSelectionDialog> dialog(new QnResourceSelectionDialog(parent));

    const auto dialogDelegate = new ExportScheduleResourceSelectionDialogDelegate(
        parent, recordingEnabled, motionUsed, dualStreamingUsed, hasVideo);

    dialog->setDelegate(dialogDelegate);

    dialog->setSelectedResources({camera->getId()});
    setHelpTopic(dialog.data(), Qn::CameraSettings_Recording_Export_Help);
    if (!dialog->exec())
        return;

    const bool copyArchiveLength = dialogDelegate->doCopyArchiveLength();
    const auto applyChanges =
        [this, sourceCamera = camera, schedule, copyArchiveLength, recordingEnabled](
            const QnVirtualCameraResourcePtr& camera)
        {
            camera->setLicenseUsed(recordingEnabled);
            const int maxFps = camera->getMaxFps();

            // TODO: #GDM #Common ask: what about constant MIN_SECOND_STREAM_FPS moving out
            // of this module or just use camera->reservedSecondStreamFps();

            int decreaseAlways = 0;
            if (camera->streamFpsSharingMethod() == Qn::BasicFpsSharing
                && camera->getMotionType() == Qn::MotionType::MT_SoftwareGrid)
            {
                decreaseAlways = QnLiveStreamParams::kMinSecondStreamFps;
            }

            int decreaseIfMotionPlusLQ = 0;
            if (camera->streamFpsSharingMethod() == Qn::BasicFpsSharing)
                decreaseIfMotionPlusLQ = QnLiveStreamParams::kMinSecondStreamFps;

            QnScheduleTaskList tasks;
            for (auto task: schedule)
            {
                if (task.recordingType == Qn::RecordingType::motionAndLow)
                    task.fps = qMin(task.fps, maxFps - decreaseIfMotionPlusLQ);
                else
                    task.fps = qMin(task.fps, maxFps - decreaseAlways);

                if (const auto bitrate = task.bitrateKbps) // Try to calculate new custom bitrate
                {
                    // Target camera supports custom bitrate
                    const auto normalBitrate =
                        nx::core::CameraBitrateCalculator::getBitrateForQualityMbps(
                            sourceCamera,
                            task.streamQuality,
                            task.fps,
                            QString()); //< Bitrate for default codec.

                    const auto bitrateAspect = (bitrate - normalBitrate) / normalBitrate;
                    const auto targetNormalBitrate =
                        nx::core::CameraBitrateCalculator::getBitrateForQualityMbps(
                            camera,
                            task.streamQuality,
                            task.fps,
                            QString()); //< Bitrate for default codec.

                    const auto targetBitrate = targetNormalBitrate * bitrateAspect;
                    task.bitrateKbps = targetBitrate;
                }

                tasks.append(task);
            }

            camera->setRecordBeforeMotionSec(sourceCamera->recordBeforeMotionSec());
            camera->setRecordAfterMotionSec(sourceCamera->recordAfterMotionSec());
            camera->setScheduleTasks(tasks);
        };

    const auto selectedCameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
        dialog->selectedResources());

    qnResourcesChangesManager->saveCameras(selectedCameras, applyChanges);
}

void QnWorkbenchResourcesSettingsHandler::at_updateLocalFilesAction_triggered()
{
    QnResourceDiscoveryManager* discoveryManager = context()->commonModule()->resourceDiscoveryManager();

    // We should update local media directories
    // Is there a better place for it?
    if (auto localFilesSearcher = commonModule()->instance<QnResourceDirectoryBrowser>())
    {
        QStringList dirs;
        dirs << qnSettings->mediaFolder();
        dirs << qnSettings->extraMediaFolders();
        localFilesSearcher->setPathCheckList(dirs);
        emit localFilesSearcher->startLocalDiscovery();
    }
}

void QnWorkbenchResourcesSettingsHandler::openLayoutSettingsDialog(
    const QnLayoutResourcePtr& layout)
{
    if (!layout)
        return;

    if (!accessController()->hasPermissions(layout, Qn::EditLayoutSettingsPermission))
        return;

    QScopedPointer<LayoutSettingsDialog> dialog(new LayoutSettingsDialog(mainWindowWidget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setLayout(layout);

    const bool backgroundWasEmpty = layout->backgroundImageFilename().isEmpty();
    if (!dialog->exec())
        return;

    // Move layout items to grid center to best fit the background.
    if (backgroundWasEmpty && !layout->backgroundImageFilename().isEmpty())
    {
        if (auto wlayout = QnWorkbenchLayout::instance(layout))
            wlayout->centralizeItems();
    }
    menu()->triggerIfPossible(action::SaveLayoutAction, layout);
}
