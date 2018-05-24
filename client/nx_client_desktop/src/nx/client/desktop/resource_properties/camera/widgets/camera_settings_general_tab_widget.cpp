#include "camera_settings_general_tab_widget.h"
#include "ui_camera_settings_general_tab_widget.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <nx/client/desktop/common/utils/aligner.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {

CameraSettingsGeneralTabWidget::CameraSettingsGeneralTabWidget(
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

    ui->wearableArchiveLengthWidget->aligner()->addAligner(
        ui->wearableMotionWidget->aligner());

    connect(store, &CameraSettingsDialogStore::stateChanged, this,
        &CameraSettingsGeneralTabWidget::loadState);

    connect(ui->cameraInfoWidget, &CameraInfoWidget::actionRequested,
        this, &CameraSettingsGeneralTabWidget::actionRequested);
}

CameraSettingsGeneralTabWidget::~CameraSettingsGeneralTabWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void CameraSettingsGeneralTabWidget::loadState(const CameraSettingsDialogState& state)
{
    ui->licensingWidget->setVisible(false);
    ui->overLicensingLine->setVisible(false);

    const bool allWearableCameras =
        state.devicesDescription.isWearable == CameraSettingsDialogState::CombinedValue::All;

    ui->wearableArchiveLengthWidget->setVisible(allWearableCameras);
    ui->wearableMotionWidget->setVisible(allWearableCameras);

    ui->rightWidget->layout()->activate();
    ui->horizontalLayout->activate();
    layout()->activate();
}

} // namespace desktop
} // namespace client
} // namespace nx
