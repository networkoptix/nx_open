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

#include <utils/common/html.h>

#include "camera_settings_tab.h"
#include "widgets/camera_settings_general_tab_widget.h"
#include "widgets/camera_schedule_widget.h"

#include "redux/camera_settings_dialog_state.h"
#include "redux/camera_settings_dialog_store.h"

#include "watchers/camera_settings_readonly_watcher.h"
#include "watchers/camera_settings_panic_watcher.h"

#include "utils/camera_settings_dialog_state_conversion_functions.h"
#include <utils/license_usage_helper.h>

namespace nx {
namespace client {
namespace desktop {

struct CameraSettingsDialog::Private
{
    QPointer<CameraSettingsDialogStore> store;
    QPointer<CameraSettingsReadOnlyWatcher> readOnlyWatcher;
    QnVirtualCameraResourceList cameras;
    QPointer<QnCamLicenseUsageHelper> licenseUsageHelper;

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
        CameraSettingsDialogStateConversionFunctions::applyStateToCameras(state, cameras);
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
}

} // namespace desktop
} // namespace client
} // namespace nx
