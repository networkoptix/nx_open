#include "camera_settings_general_tab_widget.h"
#include "ui_camera_settings_general_tab_widget.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <ui/common/read_only.h>
#include <nx/client/desktop/common/utils/aligner.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {

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

    ui->licensePanel->init(licenseUsageTextProvider, store);

    ui->wearableArchiveLengthWidget->aligner()->addAligner(
        ui->wearableMotionWidget->aligner());

    connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &CameraSettingsGeneralTabWidget::loadState);

    connect(ui->cameraInfoWidget, &CameraInfoWidget::actionRequested,
        this, &CameraSettingsGeneralTabWidget::actionRequested);

    connect(ui->licensePanel, &CameraLicensePanelWidget::actionRequested,
        this, &CameraSettingsGeneralTabWidget::actionRequested);
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

    const bool licensePanelVisible = state.devicesDescription.isIoModule != CombinedValue::None
        || state.devicesDescription.isDtsBased != CombinedValue::None;

    ui->licensePanel->setVisible(licensePanelVisible);
    ui->overLicensingLine->setVisible(licensePanelVisible);

    ::setReadOnly(ui->enableAudioCheckBox, state.readOnly);
    ::setReadOnly(ui->editCredentialsButton, state.readOnly);

    ui->rightWidget->layout()->activate();
    ui->horizontalLayout->activate();
    layout()->activate();
}

} // namespace desktop
} // namespace client
} // namespace nx
