#include "camera_settings_general_tab_widget.h"
#include "ui_camera_settings_general_tab_widget.h"
#include "../redux/camera_settings_dialog_store.h"

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

    connect(store, &CameraSettingsDialogStore::stateChanged, this,
        &CameraSettingsGeneralTabWidget::loadState);

    connect(ui->cameraInfoWidget, &CameraInfoWidget::requestPing,
        this, &CameraSettingsGeneralTabWidget::requestPing);

    connect(ui->cameraInfoWidget, &CameraInfoWidget::requestEventLog,
        this, &CameraSettingsGeneralTabWidget::requestEventLog);

    connect(ui->cameraInfoWidget, &CameraInfoWidget::requestEventRules,
        this, &CameraSettingsGeneralTabWidget::requestEventRules);

    connect(ui->cameraInfoWidget, &CameraInfoWidget::requestShowOnLayout,
        this, &CameraSettingsGeneralTabWidget::requestShowOnLayout);
}

CameraSettingsGeneralTabWidget::~CameraSettingsGeneralTabWidget()
{
}

void CameraSettingsGeneralTabWidget::loadState(const CameraSettingsDialogState& state)
{
    ui->licensingWidget->setVisible(false);
    ui->overLicensingLine->setVisible(false);
}

} // namespace desktop
} // namespace client
} // namespace nx
