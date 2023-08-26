// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_general_tab_widget.h"
#include "ui_camera_settings_general_tab_widget.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/check_box_utils.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/resource_properties/camera/dialogs/camera_streams_dialog.h>
#include <ui/common/read_only.h>

#include "../dialogs/camera_credentials_dialog.h"
#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

CameraSettingsGeneralTabWidget::CameraSettingsGeneralTabWidget(
    LicenseUsageProvider* licenseUsageProvider,
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
    ui->virtualCameraArchiveLengthWidget->setStore(store);
    ui->virtualCameraTimeZoneWidget->setStore(store);
    ui->virtualCameraMotionWidget->setStore(store);
    ui->virtualCameraUploadWidget->setStore(store);
    setHelpTopic(ui->virtualCameraArchiveLengthWidget, HelpTopic::Id::VirtualCamera);
    setHelpTopic(ui->virtualCameraTimeZoneWidget, HelpTopic::Id::VirtualCamera);
    setHelpTopic(ui->virtualCameraMotionWidget, HelpTopic::Id::VirtualCamera);
    setHelpTopic(ui->virtualCameraUploadWidget, HelpTopic::Id::VirtualCamera);

    ui->editStreamsPanel->hide();
    ui->overEditStreamsLine->hide();

    ui->licensePanel->init(licenseUsageProvider, store);

    ui->virtualCameraArchiveLengthWidget->aligner()->addAligner(
        ui->imageControlWidget->aligner());

    check_box_utils::autoClearTristate(ui->enableAudioCheckBox);
    check_box_utils::autoClearTristate(ui->enableTwoWayAudioCheckBox);

    ui->audioInputRedirectPickerWidget->setup(AudioRedirectPickerWidget::Input);
    ui->audioOutputRedirectPickerWidget->setup(AudioRedirectPickerWidget::Output);

    connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &CameraSettingsGeneralTabWidget::loadState);

    connect(ui->cameraInfoWidget, &CameraInfoWidget::actionRequested,
        this, &CameraSettingsGeneralTabWidget::actionRequested);

    connect(ui->licensePanel, &CameraLicensePanelWidget::actionRequested,
        this, &CameraSettingsGeneralTabWidget::actionRequested);

    connect(ui->enableAudioCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setAudioEnabled);

    connect(ui->audioInputRedirectPickerWidget, &AudioRedirectPickerWidget::audioRedirectDeviceIdChanged,
        store, &CameraSettingsDialogStore::setAudioInputDeviceId);

    connect(ui->enableTwoWayAudioCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setTwoWayAudioEnabled);

    connect(ui->audioOutputRedirectPickerWidget, &AudioRedirectPickerWidget::audioRedirectDeviceIdChanged,
        store, &CameraSettingsDialogStore::setAudioOutputDeviceId);

    connect(ui->virtualCameraUploadWidget, &VirtualCameraUploadWidget::actionRequested,
        this, &CameraSettingsGeneralTabWidget::actionRequested);

    connect(ui->editCredentialsButton, &QPushButton::clicked, this,
        [this, store = QPointer<CameraSettingsDialogStore>(store)]() { editCredentials(store); });

    connect(ui->editStreamsButton, &QPushButton::clicked, this,
        [this, store = QPointer<CameraSettingsDialogStore>(store)]() { editCameraStreams(store); });
}

CameraSettingsGeneralTabWidget::~CameraSettingsGeneralTabWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void CameraSettingsGeneralTabWidget::loadState(const CameraSettingsDialogState& state)
{
    const bool allVirtualCameras = state.devicesDescription.isVirtualCamera == CombinedValue::All;
    ui->virtualCameraArchiveLengthWidget->setVisible(allVirtualCameras);
    ui->virtualCameraMotionWidget->setVisible(allVirtualCameras);
    ui->virtualCameraTimeZoneWidget->setVisible(allVirtualCameras);
    ui->virtualCameraUploadWidget->setVisible(state.isSingleVirtualCamera());

    const bool licensePanelVisible = state.devicesDescription.isIoModule == CombinedValue::All
        || state.devicesDescription.isDtsBased == CombinedValue::All;

    ui->licensePanel->setVisible(licensePanelVisible);
    ui->overLicensingLine->setVisible(licensePanelVisible);
    ui->authenticationGroupBox->setVisible(
        state.devicesDescription.isVirtualCamera == CombinedValue::None);

    if (state.systemHasDevicesWithAudioInput)
        check_box_utils::setupTristateCheckbox(ui->enableAudioCheckBox, state.audioEnabled);
    else
        ui->enableAudioCheckBox->setChecked(false);

    if (state.systemHasDevicesWithAudioOutput)
        check_box_utils::setupTristateCheckbox(ui->enableTwoWayAudioCheckBox, state.twoWayAudioEnabled);
    else
        ui->enableTwoWayAudioCheckBox->setChecked(false);

    // Enable 'Enable audio' checkbox if no cameras has forced audio and at least one camera
    // supports audio.
    ui->enableAudioCheckBox->setEnabled(
        (state.isSingleCamera()
            || state.devicesDescription.supportsAudio != CombinedValue::None
            || state.audioEnabled.valueOr(true))
        && state.devicesDescription.isAudioForced == CombinedValue::None
        && state.systemHasDevicesWithAudioInput);

    ui->audioInputRedirectPickerWidget->setVisible(state.isSingleCamera()
        && state.audioEnabled.hasValue() && state.audioEnabled.get()
        && state.devicesDescription.isAudioForced == CombinedValue::None
        && state.devicesDescription.isVirtualCamera == CombinedValue::None
        && state.systemHasDevicesWithAudioInput);

    ui->audioInputRedirectPickerWidget->loadState(state);

    // Enable 'Enable two way audio' checkbox if at least one camera supports audio transmission.
    ui->enableTwoWayAudioCheckBox->setEnabled(
        (state.isSingleCamera()
            || state.devicesDescription.supportsAudioOutput != CombinedValue::None
            || state.twoWayAudioEnabled.valueOr(true))
        && state.systemHasDevicesWithAudioOutput);

    // Don't show 'Enable two way audio' checkbox if selection contain virtual cameras.
    ui->enableTwoWayAudioCheckBox->setVisible(
        state.devicesDescription.isVirtualCamera == CombinedValue::None);

    ui->audioOutputRedirectPickerWidget->setVisible(state.isSingleCamera()
        && state.twoWayAudioEnabled.hasValue() && state.twoWayAudioEnabled.get()
        && state.devicesDescription.isVirtualCamera == CombinedValue::None
        && state.systemHasDevicesWithAudioOutput);

    ui->audioOutputRedirectPickerWidget->loadState(state);

    ui->editStreamsPanel->setVisible(state.singleCameraProperties.editableStreamUrls);
    ui->overEditStreamsLine->setVisible(state.singleCameraProperties.editableStreamUrls);

    ::setReadOnly(ui->enableAudioCheckBox, state.readOnly);
    ::setReadOnly(ui->enableTwoWayAudioCheckBox, state.readOnly);
    ::setReadOnly(ui->audioInputRedirectPickerWidget, state.readOnly);
    ::setReadOnly(ui->audioOutputRedirectPickerWidget, state.readOnly);

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

    if (credentials.password.hasValue())
    {
        dialog->setPassword(credentials.password); //< Actual password value accessible to the user.
    }
    else
    {
        if (store->state().isSingleCamera())
            dialog->setHasRemotePassword(true); //< Dots placeholder, password is unknown.
        else
            dialog->setPassword(std::nullopt); //< "Multiple values" placeholder.
    }

    if (dialog->exec() == QDialog::Accepted && !dialog->hasRemotePassword())
        store->setCredentials(dialog->login(), dialog->password());
}

void CameraSettingsGeneralTabWidget::editCameraStreams(CameraSettingsDialogStore* store)
{
    if (!store)
        return;

    QScopedPointer<CameraStreamsDialog> dialog(new CameraStreamsDialog(this));
    dialog->setStreams({store->state().singleCameraSettings.primaryStream(),
        store->state().singleCameraSettings.secondaryStream()});

    if (dialog->exec() == QDialog::Accepted)
    {
        const auto streams = dialog->streams();
        store->setStreamUrls(streams.primaryStreamUrl, streams.secondaryStreamUrl);
    }
}

} // namespace nx::vms::client::desktop
