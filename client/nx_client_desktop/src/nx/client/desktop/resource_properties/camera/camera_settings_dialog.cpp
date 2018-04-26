#include "camera_settings_dialog.h"
#include "ui_camera_settings_dialog.h"

#include <QtCore/QSharedPointer>
#include <QtWidgets/QPushButton>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>

#include <ui/dialogs/common/message_box.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_selection_watcher.h>

#include <utils/common/html.h>

#include "camera_settings_tab.h"
#include "widgets/camera_settings_general_tab_widget.h"
#include "widgets/camera_schedule_widget.h"
#include "widgets/camera_motion_settings_widget.h"
#include "widgets/camera_fisheye_settings_widget.h"

#include "redux/camera_settings_dialog_state.h"
#include "redux/camera_settings_dialog_store.h"

#include "watchers/camera_settings_readonly_watcher.h"
#include "watchers/camera_settings_panic_watcher.h"

#include "utils/camera_settings_dialog_state_conversion_functions.h"
#include <utils/license_usage_helper.h>

#include <nx/client/desktop/image_providers/camera_thumbnail_manager.h>

namespace nx {
namespace client {
namespace desktop {

struct CameraSettingsDialog::Private
{
    QPointer<CameraSettingsDialogStore> store;
    QPointer<CameraSettingsReadOnlyWatcher> readOnlyWatcher;
    QnVirtualCameraResourceList cameras;
    QPointer<QnCamLicenseUsageHelper> licenseUsageHelper;
    QSharedPointer<CameraThumbnailManager> previewManager;

    bool hasChanges() const
    {
        return !cameras.empty()
            && store->state().hasChanges
            && !store->state().readOnly;
    }

    void applyChanges()
    {
        if (store->state().recording.enabled.valueOr(false))
        {
            if (!licenseUsageHelper->canEnableRecording(cameras))
                store->setRecordingEnabled(false);
        }

        store->applyChanges();
        const auto& state = store->state();

        const auto apply =
            [this, &state]
            {
                CameraSettingsDialogStateConversionFunctions::applyStateToCameras(state, cameras);
            };

        const auto backout =
            [this, camerasCopy = cameras](bool success)
            {
                if (!success && camerasCopy == cameras)
                    resetChanges();
            };

        qnResourcesChangesManager->saveCamerasBatch(cameras, apply, backout);
    }

    void resetChanges()
    {
        store->loadCameras(cameras);
    }
};

CameraSettingsDialog::CameraSettingsDialog(QWidget* parent):
    base_type(parent),
    QnSessionAwareDelegate(parent, InitializationMode::lazy),
    ui(new Ui::CameraSettingsDialog()),
    d(new Private())
{
    ui->setupUi(this);
    setButtonBox(ui->buttonBox);
    ui->alertBar->setReservedSpace(false);

    d->store = new CameraSettingsDialogStore(this);
    connect(d->store, &CameraSettingsDialogStore::stateChanged, this,
        &CameraSettingsDialog::loadState);

    d->readOnlyWatcher = new CameraSettingsReadOnlyWatcher(this);
    d->readOnlyWatcher->setStore(d->store);

    d->licenseUsageHelper = new QnCamLicenseUsageHelper(commonModule(), this);

    d->previewManager.reset(new CameraThumbnailManager());
    d->previewManager->setAutoRotate(true);
    d->previewManager->setThumbnailSize(QSize(0, 0));
    d->previewManager->setAutoRefresh(false);

    auto panicWatcher = new CameraSettingsPanicWatcher(this);
    panicWatcher->setStore(d->store);

    addPage(
        int(CameraSettingsTab::general),
        new CameraSettingsGeneralTabWidget(d->store, ui->tabWidget),
        tr("General"));

    addPage(
        int(CameraSettingsTab::recording),
        new CameraScheduleWidget(d->store, ui->tabWidget),
        tr("Recording"));

    addPage(
        int(CameraSettingsTab::motion),
        new CameraMotionSettingsWidget(d->store, ui->tabWidget),
        tr("Motion"));

    addPage(
        int(CameraSettingsTab::fisheye),
        new CameraFisheyeSettingsWidget(d->previewManager, d->store, ui->tabWidget),
        tr("Fisheye"));

    auto selectionWatcher = new QnWorkbenchSelectionWatcher(this);
    connect(
        selectionWatcher,
        &QnWorkbenchSelectionWatcher::selectionChanged,
        this,
        [this](const QnResourceList& resources)
        {
            if (isHidden())
                return;

            auto cameras = resources.filtered<QnVirtualCameraResource>();
            if (!cameras.isEmpty())
                setCameras(cameras);
        });

    // TODO: #GDM Should we handle current user permission to camera change?
    // TODO: #GDM Shouldn't this logic be incapsulated in the selection watcher?
    connect(
        resourcePool(),
        &QnResourcePool::resourceRemoved,
        this,
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
        && d->hasChanges();

    if (askConfirmation)
    {
        const auto result = showConfirmationDialog();
        switch (result)
        {
            case QDialogButtonBox::Apply:
                d->applyChanges();
                break;
            case QDialogButtonBox::Discard:
                break;
            default:
                /* Cancel changes. */
                return false;
        }
    }

    d->cameras = cameras;
    d->resetChanges();
    d->readOnlyWatcher->setCameras(cameras);
    d->previewManager->selectCamera(cameras.size() == 1
        ? cameras.front()
        : QnVirtualCameraResourcePtr());

    return true;
}

void CameraSettingsDialog::buttonBoxClicked(QDialogButtonBox::StandardButton button)
{
    base_type::buttonBoxClicked(button);
    switch (button)
    {
        case QDialogButtonBox::Ok:
        case QDialogButtonBox::Apply:
            if (d->hasChanges())
                d->applyChanges();
            break;
        case QDialogButtonBox::Cancel:
            d->resetChanges();
            break;
        default:
            break;
    }
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
        ),
        d->cameras);

    QnMessageBox messageBox(
        QnMessageBoxIcon::Question,
        tr("Apply changes before switching to another camera?"),
        extras,
        QDialogButtonBox::Apply | QDialogButtonBox::Discard | QDialogButtonBox::Cancel,
        QDialogButtonBox::Apply,
        this);

    messageBox.addCustomWidget(new QnResourceListView(d->cameras, &messageBox));
    return QDialogButtonBox::StandardButton(messageBox.exec());
}

void CameraSettingsDialog::loadState(const CameraSettingsDialogState& state)
{
    static const QString kWindowTitlePattern = lit("%1 - %2");

    const QString caption = QnCameraDeviceStringSet(
        tr("Device Settings"),
        tr("Devices Settings"),
        tr("Camera Settings"),
        tr("Cameras Settings"),
        tr("I/O Module Settings"),
        tr("I/O Modules Settings")
    ).getString(state.deviceType, state.devicesCount != 1);

    const QString description = state.devicesCount == 1
        ? state.singleCameraProperties.name()
        : QnDeviceDependentStrings::getNumericName(state.deviceType, state.devicesCount);

    setWindowTitle(kWindowTitlePattern.arg(caption).arg(description));

    if (buttonBox())
    {
        const auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
        const auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

        if (okButton)
            okButton->setEnabled(!state.readOnly);

        if (applyButton)
            applyButton->setEnabled(!state.readOnly && state.hasChanges);
    }

    setPageVisible(int(CameraSettingsTab::motion), state.isSingleCamera()
        && state.devicesDescription.hasMotion == CameraSettingsDialogState::CombinedValue::All);

    setPageVisible(int(CameraSettingsTab::fisheye), state.isSingleCamera());

    ui->alertBar->setText(getAlertText(state));
}

QString CameraSettingsDialog::getAlertText(const CameraSettingsDialogState& state)
{
    if (!state.alert)
        return QString();

    using Alert = CameraSettingsDialogState::Alert;
    switch (*state.alert)
    {
        case Alert::BrushChanged:
            return tr("Select areas on the schedule to apply chosen parameters to.");

        case Alert::EmptySchedule:
            return tr(
                "Set recording parameters and select areas "
                "on the schedule grid to apply them to.");

        case Alert::NotEnoughLicenses:
            return tr("Not enough licenses to enable recording.");

        case Alert::LicenseLimitExceeded:
            return tr("License limit exceeded, recording will not be enabled.");

        case Alert::RecordingIsNotEnabled:
            return tr("Turn on selector at the top of the window to enable recording.");

        case Alert::HighArchiveLength:
            return QnCameraDeviceStringSet(
                    tr("High minimum value can lead to archive length decrease on other devices."),
                    tr("High minimum value can lead to archive length decrease on other cameras."))
                .getString(state.deviceType);

        case Alert::MotionDetectionRequiresRecording:
            return tr(
                "Motion detection will work only when camera is being viewed. "
                "Enable recording to make it work all the time.");

        case Alert::MotionDetectionTooManyRectangles:
            return tr("Maximum number of motion detection rectangles for current camera is reached");

        case Alert::MotionDetectionTooManyMaskRectangles:
            return tr("Maximum number of ignore motion rectangles for current camera is reached");

        case Alert::MotionDetectionTooManySensitivityRectangles:
            return tr("Maximum number of detect motion rectangles for current camera is reached");

        default:
            NX_EXPECT(false, "Unhandled enum value");
            break;
    }

    return QString();
}

} // namespace desktop
} // namespace client
} // namespace nx
