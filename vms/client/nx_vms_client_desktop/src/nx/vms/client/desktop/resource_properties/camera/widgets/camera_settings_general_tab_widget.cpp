#include "camera_settings_general_tab_widget.h"
#include "ui_camera_settings_general_tab_widget.h"
#include "../dialogs/camera_credentials_dialog.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <ui/common/read_only.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/check_box_utils.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

CameraSettingsGeneralTabWidget::CameraSettingsGeneralTabWidget(
    AbstractTextProvider* licenseUsageTextProvider,
    CameraSettingsDialogStore* store,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::CameraSettingsGeneralTabWidget())
{
    NX_ASSERT(store);

    ui->setupUi(this);
    ui->cameraInfoWidget->setStore(store);
    ui->imageControlWidget->setStore(store);
    ui->wearableArchiveLengthWidget->setStore(store);
    ui->wearableMotionWidget->setStore(store);
    ui->wearableUploadWidget->setStore(store);

    ui->licensePanel->init(licenseUsageTextProvider, store);

    ui->wearableArchiveLengthWidget->aligner()->addAligner(
        ui->imageControlWidget->aligner());

    check_box_utils::autoClearTristate(ui->enableAudioCheckBox);

    connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &CameraSettingsGeneralTabWidget::loadState);

    connect(ui->cameraInfoWidget, &CameraInfoWidget::actionRequested,
        this, &CameraSettingsGeneralTabWidget::actionRequested);

    connect(ui->licensePanel, &CameraLicensePanelWidget::actionRequested,
        this, &CameraSettingsGeneralTabWidget::actionRequested);

    connect(ui->enableAudioCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setAudioEnabled);

    connect(ui->wearableUploadWidget, &WearableCameraUploadWidget::actionRequested,
        this, &CameraSettingsGeneralTabWidget::actionRequested);

    connect(ui->editCredentialsButton, &QPushButton::clicked, this,
        [this, store = QPointer<CameraSettingsDialogStore>(store)]() { editCredentials(store); });
}

CameraSettingsGeneralTabWidget::~CameraSettingsGeneralTabWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void CameraSettingsGeneralTabWidget::loadState(const CameraSettingsDialogState& state)
{
    using CombinedValue = CameraSettingsDialogState::CombinedValue;

    const bool allWearableCameras = state.devicesDescription.isWearable == CombinedValue::All;
    ui->wearableArchiveLengthWidget->setVisible(allWearableCameras);
    ui->wearableMotionWidget->setVisible(allWearableCameras);
    ui->wearableUploadWidget->setVisible(state.isSingleWearableCamera());

    const bool licensePanelVisible = state.devicesDescription.isIoModule != CombinedValue::None
        || state.devicesDescription.isDtsBased != CombinedValue::None;

    ui->licensePanel->setVisible(licensePanelVisible);
    ui->overLicensingLine->setVisible(licensePanelVisible);
    ui->authenticationGroupBox->setVisible(
        state.devicesDescription.isWearable == CombinedValue::None);

    check_box_utils::setupTristateCheckbox(ui->enableAudioCheckBox, state.audioEnabled);

    // Enable checkbox if no cameras has forced audio and at least one camera supports audio.
    ui->enableAudioCheckBox->setEnabled(
        state.devicesDescription.supportsAudio != CombinedValue::None
        && state.devicesDescription.isAudioForced == CombinedValue::None);

    // Hide audio checkbox for NVRs and IO modules (devices with forced audio).
    const bool hasAudioSettings = state.devicesDescription.isAudioForced != CombinedValue::All;
    ui->enableAudioCheckBox->setVisible(hasAudioSettings);
    ui->audioGroupBox->setVisible(hasAudioSettings);

    ::setReadOnly(ui->enableAudioCheckBox, state.readOnly);
    ::setReadOnly(ui->editCredentialsButton, state.readOnly);

    ui->rightWidget->layout()->activate();
    ui->horizontalLayout->activate();
    layout()->activate();
}

void CameraSettingsGeneralTabWidget::editCredentials(CameraSettingsDialogStore* store)
{
    if (!store)
        return;

    QScopedPointer<CameraCredentialsDialog> dialog(new CameraCredentialsDialog(this));

    const auto& credentials = store->state().credentials;
    dialog->setLogin(credentials.login);
    dialog->setPassword(credentials.password);

    if (dialog->exec() == QDialog::Accepted)
        store->setCredentials(dialog->login(), dialog->password());
}

} // namespace nx::vms::client::desktop
