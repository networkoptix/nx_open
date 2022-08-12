// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_resources_settings_handler.h"

#include <QtWidgets/QAction>

#include <client/client_module.h>
#include <client/client_settings.h>
#include <common/common_module.h>
#include <core/misc/schedule_task.h>
#include <core/resource/camera_resource.h>
#include <core/resource/fake_media_server.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/desktop/debug_utils/utils/debug_custom_actions.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource_dialogs/copy_schedule_camera_selection_dialog.h>
#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_dialog.h>
#include <nx/vms/client/desktop/resource_properties/camera/flux/camera_settings_dialog_state.h>
#include <nx/vms/client/desktop/resource_properties/layout/flux/layout_settings_dialog_state_reducer.h>
#include <nx/vms/client/desktop/resource_properties/layout/layout_settings_dialog.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/utils/parameter_helper.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/dialogs/resource_properties/user_roles_dialog.h>
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/camera/camera_bitrate_calculator.h>

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
        [](auto /*context*/) { LayoutSettingsDialogStateReducer::setTracingEnabled(true); });
}

QnWorkbenchResourcesSettingsHandler::~QnWorkbenchResourcesSettingsHandler()
{
}

void QnWorkbenchResourcesSettingsHandler::at_cameraSettingsAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto cameras = parameters.resources().filtered<QnVirtualCameraResource>(
        [](const QnResourcePtr& resource) { return resource && !resource->hasFlags(Qn::removed); });

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
    const QnMediaServerResourceList servers = params.resources().filtered<QnMediaServerResource>(
        [](const QnMediaServerResourcePtr& server)
        {
            return server && !server->hasFlags(Qn::removed) && !server->hasFlags(Qn::fake);
        });

    NX_ASSERT(servers.size() == 1, "Invalid action condition");
    if (servers.isEmpty())
        return;

    QnMediaServerResourcePtr server = servers.first();

    const bool hasAccess = accessController()->hasGlobalPermission(GlobalPermission::admin);
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
    QnUserResourcePtr user(new QnUserResource(nx::vms::api::UserType::local));
    user->setRawPermissions(GlobalPermission::liveViewerPermissions);

    // Shows New User dialog as modal because we can't pick anothr user from resources tree anyway.
    if (m_userSettingsDialog)
        m_userSettingsDialog->close();

    const auto params = menu()->currentParameters(sender());
    const auto parent = utils::extractParentWidget(params, mainWindowWidget());

    const QScopedPointer<QnUserSettingsDialog> dialog(new QnUserSettingsDialog(parent));
    dialog->setUser(user);
    dialog->setCurrentPage(QnUserSettingsDialog::SettingsPage);
    dialog->forcedUpdate();
    dialog->exec();
}

void QnWorkbenchResourcesSettingsHandler::at_userSettingsAction_triggered()
{
    const auto params = menu()->currentParameters(sender());
    const QnUserResourcePtr user = params.resource().dynamicCast<QnUserResource>();
    if (!user || user->hasFlags(Qn::removed))
        return;

    const bool hasAccess = accessController()->hasPermissions(user, Qn::ReadPermission);
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

    const bool copyArchiveLength = dialog->copyArchiveLength();
    const auto applyChanges =
        [this, &settings, copyArchiveLength](
            const QnVirtualCameraResourcePtr& camera)
        {
            using CameraBitrateCalculator = nx::vms::common::CameraBitrateCalculator;

            if (NX_ASSERT(settings.recording.enabled.hasValue()))
                camera->setLicenseUsed(settings.recording.enabled());

            const int maxFps = camera->getMaxFps();
            const int reservedSecondStreamFps = camera->reservedSecondStreamFps();

            // Maximum fps should always been decreased by this value.
            const int decreaseAlways = camera->getMotionType() == MotionType::software
                ? reservedSecondStreamFps
                : 0;

            // Fps in the Motion+LQ cells should be decreased by this value.
            const int decreaseIfMotionPlusLQ = reservedSecondStreamFps;

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
                if (task.recordingType == RecordingType::metadataAndLowQuality)
                    task.fps = qMin(task.fps, maxFps - decreaseIfMotionPlusLQ);
                else
                    task.fps = qMin(task.fps, maxFps - decreaseAlways);

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

                tasks.append(task);
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

void QnWorkbenchResourcesSettingsHandler::at_updateLocalFilesAction_triggered()
{
    // We should update local media directories
    // Is there a better place for it?
    if (auto localFilesSearcher = qnClientModule->resourceDirectoryBrowser())
        localFilesSearcher->setLocalResourcesDirectories(qnSettings->mediaFolders());
}

void QnWorkbenchResourcesSettingsHandler::openLayoutSettingsDialog(
    const QnLayoutResourcePtr& layout)
{
    if (!layout || layout->hasFlags(Qn::removed))
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
