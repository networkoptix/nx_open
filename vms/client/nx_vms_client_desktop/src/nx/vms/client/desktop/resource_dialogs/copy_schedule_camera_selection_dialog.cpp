// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "copy_schedule_camera_selection_dialog.h"

#include <QtWidgets/QCheckBox>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/misc/schedule_task.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_properties/camera/flux/camera_settings_dialog_state.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/license/usage_helper.h>

namespace nx::vms::client::desktop {

namespace {

CameraSelectionDialog::AlertTextProvider initAlertTextProvider(
    const CameraSettingsDialogState& settings)
{
    return
        [&settings](const QSet<QnResourcePtr>& resources) -> QString
        {
            QStringList alertRows;

            const bool hasVideo = NX_ASSERT(settings.isSingleCamera())
                ? settings.singleCameraProperties.hasVideo
                : settings.devicesDescription.supportsVideo == CombinedValue::All;

            QnVirtualCameraResourceList cameras;
            for (const auto& resource: resources)
            {
                if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
                    cameras.append(camera);
            }

            if (NX_ASSERT(settings.recording.enabled.hasValue())
                && settings.recording.enabled())
            {
                using namespace nx::vms::license;
                CamLicenseUsageHelper helper(
                    cameras,
                    /*enableRecording*/ true,
                    qnClientCoreModule->commonModule()->systemContext());

                if (!helper.isValid())
                {
                    alertRows.append(helper.getProposedUsageMsg());
                    alertRows.append(helper.getRequiredMsg());
                    alertRows.append(CopyScheduleCameraSelectionDialog::tr(
                        "Recording will not be enabled on some cameras."));
                }
            }

            const bool canApplySchedule = std::all_of(cameras.cbegin(), cameras.cend(),
                [schedule = settings.recording.schedule()]
                    (const QnVirtualCameraResourcePtr& camera)
                {
                    return camera->canApplySchedule(schedule);
                });
            if (!canApplySchedule)
            {
                alertRows.append(CopyScheduleCameraSelectionDialog::tr(
                    "Recording Schedule contains recording modes that are not supported by the "
                    "selected cameras. Unsupported recording modes will be changed to \"Record "
                    "Always\"."));
            }

            const bool dtsCamPresent = std::any_of(cameras.cbegin(), cameras.cend(),
                [](const QnVirtualCameraResourcePtr& camera) { return camera->isDtsBased(); });
            if (dtsCamPresent)
            {
                alertRows.append(CopyScheduleCameraSelectionDialog::tr(
                    "Recording cannot be enabled for some cameras."));
            }

            // If source camera has no video, allow to copy only to cameras without video.
            const bool videoOk = hasVideo || !std::any_of(cameras.cbegin(), cameras.cend(),
                [](const QnVirtualCameraResourcePtr& camera) { return camera->hasVideo(); });
            if (!videoOk)
            {
                alertRows.append(CopyScheduleCameraSelectionDialog::tr(
                    "Schedule settings are not compatible with some devices."));
            }
            return alertRows.join('\n');
        };
}

CameraSelectionDialog::ResourceFilter supportsSchedule(const CameraSettingsDialogState& settings)
{
    return
        [selfId = settings.singleCameraId()](const QnResourcePtr& resource)
        {
            // Hide source camera from the tree.
            if (resource->getId() == selfId)
                return false;

            if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
                return camera->supportsSchedule();

            return false;
        };
}

CameraSelectionDialog::ResourceValidator canApplySchedule(
    const CameraSettingsDialogState& settings)
{
    return
        [schedule = settings.recording.schedule()](const QnResourcePtr& resource)
        {
            if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
                return camera->canApplySchedule(schedule);

            return false;
        };
}

QWidget* createCheckBoxFooterWidget()
{
    const auto mainLayout = new QVBoxLayout();
    mainLayout->setSpacing(0);

    const auto horizontalLine = new QFrame();
    horizontalLine->setFixedHeight(2);
    horizontalLine->setFrameShadow(QFrame::Sunken);
    horizontalLine->setFrameShape(QFrame::HLine);
    mainLayout->addWidget(horizontalLine);

    const auto checkboxLayout = new QVBoxLayout();
    checkboxLayout->setContentsMargins(
        style::Metrics::kDefaultTopLevelMargin,
        style::Metrics::kStandardPadding,
        style::Metrics::kDefaultTopLevelMargin,
        style::Metrics::kStandardPadding);

    checkboxLayout->addWidget(new QCheckBox());
    mainLayout->addLayout(checkboxLayout);

    QWidget* footerWidget = new QWidget();
    footerWidget->setLayout(mainLayout);

    return footerWidget;
}

} // namespace

CopyScheduleCameraSelectionDialog::CopyScheduleCameraSelectionDialog(
    const CameraSettingsDialogState& settings,
    QWidget* parent)
    :
    base_type(
        /*resourceFilter*/ supportsSchedule(settings),
        /*resourceValidator*/ canApplySchedule(settings),
        /*alertTextProvider*/ initAlertTextProvider(settings),
        parent)
{
    resourceSelectionWidget()->setShowRecordingIndicator(true);
    setHelpTopic(this, HelpTopic::Id::CameraSettings_Recording_Export);

    resourceSelectionWidget()->resourceViewWidget()->setFooterWidget(createCheckBoxFooterWidget());
    copyArchiveLengthCheckBox()->setText(tr("Copy archive length settings"));
}

bool CopyScheduleCameraSelectionDialog::copyArchiveLength() const
{
    return copyArchiveLengthCheckBox()->isChecked();
}

void CopyScheduleCameraSelectionDialog::setCopyArchiveLength(bool copyArchiveLength)
{
    copyArchiveLengthCheckBox()->setChecked(copyArchiveLength);
}

QCheckBox* CopyScheduleCameraSelectionDialog::copyArchiveLengthCheckBox() const
{
    return resourceSelectionWidget()->resourceViewWidget()->footerWidget()->findChild<QCheckBox*>();
}

} // namespace nx::vms::client::desktop
