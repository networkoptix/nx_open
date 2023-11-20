// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_resources_settings_handler.h"

#include <QtGui/QAction>

#include <client/client_module.h>
#include <common/common_module.h>
#include <core/misc/schedule_task.h>
#include <core/resource/camera_media_stream_info.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/debug_utils/utils/debug_custom_actions.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource_dialogs/copy_schedule_camera_selection_dialog.h>
#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_dialog.h>
#include <nx/vms/client/desktop/resource_properties/camera/flux/camera_settings_dialog_state.h>
#include <nx/vms/client/desktop/resource_properties/layout/flux/layout_settings_dialog_state_reducer.h>
#include <nx/vms/client/desktop/resource_properties/layout/layout_settings_dialog.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/parameter_helper.h>
#include <nx/vms/client/desktop/workbench/managers/settings_dialog_manager.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/license/usage_helper.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/workbench/watchers/workbench_selection_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/camera/camera_bitrate_calculator.h>

using namespace nx::vms::client::desktop;

namespace {

// TODO: #sivanov Implement context-aware permissions check.
QnVirtualCameraResourceList settingsAvailableCameras(const QnResourceList& resources)
{
    return resources.filtered<QnVirtualCameraResource>(
        [](const QnVirtualCameraResourcePtr& camera)
        {
            return camera
                && !camera->hasFlags(Qn::cross_system)
                && !camera->hasFlags(Qn::removed);
        });
}

} // namespace

QnWorkbenchResourcesSettingsHandler::QnWorkbenchResourcesSettingsHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(menu::CameraSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_cameraSettingsAction_triggered);
    connect(action(menu::ServerSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_serverSettingsAction_triggered);
    connect(action(menu::NewUserAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_newUserAction_triggered);
    connect(action(menu::UserSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_userSettingsAction_triggered);
    connect(action(menu::UserGroupsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_userGroupsAction_triggered);
    connect(action(menu::LayoutSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_layoutSettingsAction_triggered);
    connect(action(menu::CurrentLayoutSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_currentLayoutSettingsAction_triggered);
    connect(action(menu::CopyRecordingScheduleAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_copyRecordingScheduleAction_triggered);

    registerDebugAction(
        "Tracing: Layout settings",
        [](auto /*context*/) { LayoutSettingsDialogStateReducer::setTracingEnabled(true); });

    // TODO: #sivanov Move out code from user settings and server settings dialogs here.
    auto selectionWatcher = new QnWorkbenchSelectionWatcher(this);
    connect(selectionWatcher, &QnWorkbenchSelectionWatcher::selectionChanged, this,
        [this](const QnResourceList& resources)
        {
            auto cameras = settingsAvailableCameras(resources);

            if (!cameras.isEmpty()
                && m_cameraSettingsDialog
                && !m_cameraSettingsDialog->isHidden())
            {
                m_cameraSettingsDialog->setCameras(cameras);
            }
        });
}

QnWorkbenchResourcesSettingsHandler::~QnWorkbenchResourcesSettingsHandler()
{
}

void QnWorkbenchResourcesSettingsHandler::at_cameraSettingsAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto cameras = settingsAvailableCameras(parameters.resources());
    if (cameras.isEmpty())
        return;

    const auto parent = utils::extractParentWidget(parameters, mainWindowWidget());

    QnNonModalDialogConstructor<CameraSettingsDialog> dialogConstructor(
        m_cameraSettingsDialog,
        [this, parent] { return new CameraSettingsDialog(systemContext(), parent); });

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
    const QnMediaServerResourceList servers = params.resources().filtered<QnMediaServerResource>(
        [](const QnMediaServerResourcePtr& server)
        {
            return server && !server->hasFlags(Qn::removed);
        });

    NX_ASSERT(servers.size() == 1, "Invalid action condition");
    if (servers.isEmpty())
        return;

    QnMediaServerResourcePtr server = servers.first();

    const bool hasAccess = accessController()->hasPowerUserPermissions();
    NX_ASSERT(hasAccess, "Invalid action condition"); /*< It must be checked on action level. */
    if (!hasAccess)
        return;

    const auto parent = utils::extractParentWidget(params, mainWindowWidget());
    QnNonModalDialogConstructor<QnServerSettingsDialog> dialogConstructor(
        m_serverSettingsDialog, parent);

    dialogConstructor.disableAutoFocus();

    m_serverSettingsDialog->setServer(server);
    if (params.hasArgument(Qn::FocusTabRole))
        m_serverSettingsDialog->setCurrentPage(params.argument<int>(Qn::FocusTabRole));
}

void QnWorkbenchResourcesSettingsHandler::at_newUserAction_triggered()
{
    // Shows New User dialog as modal.

    const auto params = menu()->currentParameters(sender());
    const auto parent = utils::extractParentWidget(params, mainWindowWidget());

    context()->settingsDialogManager()->createUser(parent);
}

void QnWorkbenchResourcesSettingsHandler::at_userSettingsAction_triggered()
{
    const auto params = menu()->currentParameters(sender());
    const auto user = params.resource().dynamicCast<QnUserResource>();
    if (!user || user->hasFlags(Qn::removed))
        return;

    const bool hasAccess = accessController()->hasPermissions(user, Qn::ReadPermission);
    NX_ASSERT(hasAccess, "Invalid action condition");
    if (!hasAccess)
        return;

    const auto parent = utils::extractParentWidget(params, mainWindowWidget());

    bool hasTab = false;
    const auto tab = params.argument(Qn::FocusTabRole).toInt(&hasTab);

    context()->settingsDialogManager()->editUser(user->getId(), hasTab ? tab : -1, parent);
}

void QnWorkbenchResourcesSettingsHandler::at_userGroupsAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto parent = utils::extractParentWidget(parameters, mainWindowWidget());
    const auto groupId = parameters.argument(Qn::UuidRole).value<QnUuid>();

    if (groupId.isNull())
        context()->settingsDialogManager()->createGroup(parent);
    else
        context()->settingsDialogManager()->editGroup(groupId, parent);
}

void QnWorkbenchResourcesSettingsHandler::at_layoutSettingsAction_triggered()
{
    const auto params = menu()->currentParameters(sender());
    openLayoutSettingsDialog(params.resource().dynamicCast<LayoutResource>());
}

void QnWorkbenchResourcesSettingsHandler::at_currentLayoutSettingsAction_triggered()
{
    openLayoutSettingsDialog(workbench()->currentLayout()->resource());
}

void QnWorkbenchResourcesSettingsHandler::at_copyRecordingScheduleAction_triggered()
{
    using namespace nx::vms::api;

    if (!NX_ASSERT(m_cameraSettingsDialog))
        return;

    // More correct way would be to pass settings using action parameters, but state class is not
    // copyable, therefore we cannot declare metatype for it.
    const CameraSettingsDialogState& settings = m_cameraSettingsDialog->state();

    if (!NX_ASSERT(settings.recording.schedule.hasValue()))
        return;

    QScopedPointer<CopyScheduleCameraSelectionDialog> dialog(
        new CopyScheduleCameraSelectionDialog(settings, m_cameraSettingsDialog.data()));

    if (!dialog->exec())
        return;

    const bool proposeToEnableRecording = NX_ASSERT(settings.recording.enabled.hasValue())
        ? settings.recording.enabled()
        : false;

    nx::vms::license::CamLicenseUsageHelper licenseUsageHelper(systemContext());

    const bool copyArchiveLength = dialog->copyArchiveLength();
    const auto applyChanges =
        [this, &settings, &licenseUsageHelper, proposeToEnableRecording, copyArchiveLength](
            const QnVirtualCameraResourcePtr& camera)
        {
            using CameraBitrateCalculator = nx::vms::common::CameraBitrateCalculator;

            licenseUsageHelper.propose(camera, proposeToEnableRecording);
            camera->setScheduleEnabled(proposeToEnableRecording
                && !licenseUsageHelper.isOverflowForCamera(camera));

            const int maxFps = camera->getMaxFps();
            const int reservedSecondStreamFps =
                camera->hasDualStreaming() ? camera->reservedSecondStreamFps() : 0;

            const QnScheduleTaskList schedule = settings.recording.schedule();

            QnScheduleTaskList tasks;
            for (auto task: schedule)
            {
                if (!camera->canApplyScheduleTask(task))
                {
                    task.recordingType = RecordingType::always;
                    task.metadataTypes = {};
                }

                const int originalFps = task.fps;
                task.fps = qMin(task.fps, maxFps - reservedSecondStreamFps);

                // Try to calculate new custom bitrate.
                if (const auto bitrate = task.bitrateKbps)
                {
                    // Target camera supports custom bitrate
                    const auto normalBitrate = CameraBitrateCalculator::roundKbpsToMbps(
                        CameraBitrateCalculator::suggestBitrateForQualityKbps(
                            task.streamQuality,
                            settings.recording.defaultStreamResolution,
                            originalFps,
                            QString(), //< Calculate bitrate for default codec.
                            settings.recording.mediaStreamCapability,
                            settings.recording.useBitratePerGop));

                    const auto bitrateAspect = (bitrate - normalBitrate) / normalBitrate;
                    const auto targetNormalBitrate = CameraBitrateCalculator::roundKbpsToMbps(
                        camera->suggestBitrateForQualityKbps(
                            task.streamQuality,
                            camera->streamInfo().getResolution(),
                            task.fps,
                            QString())); //< Bitrate for default codec.

                    const auto targetBitrate = targetNormalBitrate * bitrateAspect;
                    task.bitrateKbps = targetBitrate;
                }

                tasks.push_back(task);
            }
            camera->setScheduleTasks(tasks);

            if (auto beforeSec = settings.recording.thresholds.beforeSec; beforeSec.hasValue())
                camera->setRecordBeforeMotionSec(beforeSec());
            if (auto afterSec = settings.recording.thresholds.afterSec; afterSec.hasValue())
                camera->setRecordAfterMotionSec(afterSec());

            if (copyArchiveLength)
            {
                camera->setMinPeriod(settings.recording.minPeriod.rawValue());
                camera->setMaxPeriod(settings.recording.maxPeriod.rawValue());
            }
        };

    auto selectedCameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
        dialog->resourceSelectionWidget()->selectedResourcesIds());

    qnResourcesChangesManager->saveCameras(selectedCameras, applyChanges);
}

void QnWorkbenchResourcesSettingsHandler::openLayoutSettingsDialog(
    const LayoutResourcePtr& layout)
{
    if (!layout || layout->hasFlags(Qn::removed))
        return;

    if (!ResourceAccessManager::hasPermissions(layout, Qn::EditLayoutSettingsPermission))
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
        if (auto wlayout = workbench()->layout(layout))
            wlayout->centralizeItems();
    }
    menu()->triggerIfPossible(menu::SaveLayoutAction, layout);
}
