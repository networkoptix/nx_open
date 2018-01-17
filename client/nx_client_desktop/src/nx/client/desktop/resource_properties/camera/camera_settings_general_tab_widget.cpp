#include "camera_settings_general_tab_widget.h"
#include "ui_camera_settings_general_tab_widget.h"

#include "camera_settings_model.h"

namespace nx {
namespace client {
namespace desktop {

CameraSettingsGeneralTabWidget::CameraSettingsGeneralTabWidget(
    CameraSettingsModel* model,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::CameraSettingsGeneralTabWidget()),
    m_model(model)
{
    ui->setupUi(this);
    ui->cameraInfoWidget->setModel(model);

    ui->licensingWidget->setVisible(false);
    ui->overLicensingLine->setVisible(false);
}

CameraSettingsGeneralTabWidget::~CameraSettingsGeneralTabWidget()
{

}

bool CameraSettingsGeneralTabWidget::hasChanges() const
{
    return false;
}

void CameraSettingsGeneralTabWidget::loadDataToUi()
{
    ui->cameraInfoWidget->loadDataToUi();

    ui->imageControlWidget->updateFromResources(m_model->cameras());
}

void CameraSettingsGeneralTabWidget::applyChanges()
{
    ui->imageControlWidget->submitToResources(m_model->cameras());
}

} // namespace desktop
} // namespace client
} // namespace nx
