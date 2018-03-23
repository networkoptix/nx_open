#include "camera_settings_dialog.h"
#include "ui_camera_settings_dialog.h"

#include <QtWidgets/QPushButton>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>

#include <ui/dialogs/common/message_box.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_selection_watcher.h>
#include <ui/workbench/watchers/workbench_safemode_watcher.h>

#include <utils/common/html.h>

#include "camera_settings_tab.h"
#include "camera_settings_general_tab_widget.h"
#include "widgets/camera_schedule_widget.h"

#include "redux/camera_settings_dialog_state.h"
#include "redux/camera_settings_dialog_store.h"

namespace nx {
namespace client {
namespace desktop {

struct CameraSettingsDialog::Private
{
    QPointer<CameraSettingsDialogStore> store;
    QnVirtualCameraResourceList cameras;


    using State = CameraSettingsDialogState;

    void setMinRecordingDays(const State::RecordingDays& value)
    {
        if (!value.same)
            return;
        int actualValue = value.absoluteValue;
        NX_ASSERT(actualValue > 0);
        if (actualValue == 0)
            actualValue = ec2::kDefaultMinArchiveDays;
        if (value.automatic)
            actualValue = -actualValue;

        for (const auto& camera: cameras)
            camera->setMinDays(actualValue);
    }

    void setMaxRecordingDays(const State::RecordingDays& value)
    {
        if (!value.same)
            return;
        int actualValue = value.absoluteValue;
        NX_ASSERT(actualValue > 0);
        if (actualValue == 0)
            actualValue = ec2::kDefaultMaxArchiveDays;
        if (value.automatic)
            actualValue = -actualValue;

        for (const auto& camera: cameras)
            camera->setMaxDays(actualValue);
    }

    void submitStateToCameras()
    {
        const auto& state = store->state();
        NX_ASSERT(!state.hasChanges);
        if (state.isSingleCamera())
        {
            cameras.first()->setName(state.singleCameraSettings.name());
        }
        setMinRecordingDays(state.recording.minDays);
        setMaxRecordingDays(state.recording.maxDays);
    }

};

CameraSettingsDialog::CameraSettingsDialog(QWidget* parent):
    base_type(parent),
    QnSessionAwareDelegate(parent, InitializationMode::lazy),
    ui(new Ui::CameraSettingsDialog()),
    d(new Private())
{
    d->store = new CameraSettingsDialogStore(this);

    connect(
        d->store,
        &CameraSettingsDialogStore::stateChanged,
        this,
        &CameraSettingsDialog::loadState);

    ui->setupUi(this);

    addPage(int(CameraSettingsTab::general),
        new CameraSettingsGeneralTabWidget(d->store, this),
        tr("General"));

    auto cameraScheduleWidget = new CameraScheduleWidget(d->store, ui->tabWidget);
    addPage(int(CameraSettingsTab::recording),
        cameraScheduleWidget,
        tr("Recording"));

    auto selectionWatcher = new QnWorkbenchSelectionWatcher(this);
    connect(selectionWatcher, &QnWorkbenchSelectionWatcher::selectionChanged, this,
        [this](const QnResourceList& resources)
        {
            if (isHidden())
                return;

            auto cameras = resources.filtered<QnVirtualCameraResource>();
            if (!cameras.isEmpty())
                setCameras(cameras);
        });


    // TODO: #GDM Should we handle current user permission to camera change?
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
            if (!camera)
                return;

            if (d->cameras.contains(camera))
            {
                auto camerasLeft = d->cameras;
                camerasLeft.removeAll(camera);
                setCameras(camerasLeft, true);
                if (camerasLeft.empty())
                    tryClose(true);
            }
        });

    ui->alertBar->setReservedSpace(false);

    const auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    const auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

    auto safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(ui->buttonBox);
    safeModeWatcher->addControlledWidget(okButton,
        QnWorkbenchSafeModeWatcher::ControlMode::Disable);

    // Hiding Apply button, otherwise it will be enabled in the QnGenericTabbedDialog code.
    safeModeWatcher->addControlledWidget(applyButton,
        QnWorkbenchSafeModeWatcher::ControlMode::Hide);
}

CameraSettingsDialog::~CameraSettingsDialog() = default;

bool CameraSettingsDialog::tryClose(bool force)
{
    auto result = setCameras({}, force);
    result |= force;
    if (result)
        hide();

    return result;
}

void CameraSettingsDialog::forcedUpdate()
{
}

bool CameraSettingsDialog::setCameras(const QnVirtualCameraResourceList& cameras, bool force)
{
    const bool askConfirmation =
        !force
        && isVisible()
        && d->cameras != cameras
        && !d->cameras.empty()
        && d->store->state().hasChanges;

    if (askConfirmation)
    {
        const auto result = showConfirmationDialog();
        switch (result)
        {
            case QDialogButtonBox::Apply:
                d->store->applyChanges();
                d->submitStateToCameras();
                break;
            case QDialogButtonBox::Discard:
                break;
            default:
                /* Cancel changes. */
                return false;
        }

    }

    d->cameras = cameras;
    d->store->loadCameras(d->cameras);
    return true;
}

QDialogButtonBox::StandardButton CameraSettingsDialog::showConfirmationDialog()
{
    if (d->cameras.empty())
        return QDialogButtonBox::Discard;

    const auto extras = QnDeviceDependentStrings::getNameFromSet(
        resourcePool(),
        QnCameraDeviceStringSet(
            tr("Changes to the following %n devices are not saved:", "", d->cameras.size()),
            tr("Changes to the following %n cameras are not saved:", "", d->cameras.size()),
            tr("Changes to the following %n I/O Modules are not saved:", "", d->cameras.size())
        ), d->cameras);

    QnMessageBox messageBox(QnMessageBoxIcon::Question,
        tr("Apply changes before switching to another camera?"), extras,
        QDialogButtonBox::Apply | QDialogButtonBox::Discard | QDialogButtonBox::Cancel,
        QDialogButtonBox::Apply, this);

    messageBox.addCustomWidget(new QnResourceListView(d->cameras, &messageBox));
    return QDialogButtonBox::StandardButton(messageBox.exec());
}

void CameraSettingsDialog::loadState(const CameraSettingsDialogState& state)
{
    static const QString kWindowTitlePattern = lit("%1 - %2");

    const QString caption = QnCameraDeviceStringSet(
            tr("Device Settings"), tr("Devices Settings"),
            tr("Camera Settings"), tr("Cameras Settings"),
            tr("I/O Module Settings"), tr("I/O Modules Settings")
        ).getString(state.deviceType, state.devicesCount != 1);

    const QString description = state.devicesCount == 1
        ? state.singleCameraSettings.name.get()
        : QnDeviceDependentStrings::getNumericName(state.deviceType, state.devicesCount);

    setWindowTitle(kWindowTitlePattern.arg(caption).arg(description));
}

} // namespace desktop
} // namespace client
} // namespace nx
