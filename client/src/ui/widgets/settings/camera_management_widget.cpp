#include "camera_management_widget.h"
#include "ui_camera_management_widget.h"

#include <api/global_settings.h>

QnCameraManagementWidget::QnCameraManagementWidget(QWidget *parent):
    QWidget(parent),
    ui(new Ui::CameraManagementWidget)
{
    ui->setupUi(this);
}

QnCameraManagementWidget::~QnCameraManagementWidget() {
    return;
}

void QnCameraManagementWidget::updateFromSettings() {
    QnGlobalSettings *settings = QnGlobalSettings::instance();

    ui->autoDiscoveryCheckBox->setChecked(settings->isCameraAutoDiscoveryEnabled());
    ui->autoSettingsCheckBox->setChecked(settings->isCameraSettingsOptimizationEnabled());
}

void QnCameraManagementWidget::submitToSettings() {
    QnGlobalSettings *settings = QnGlobalSettings::instance();
    
    settings->setCameraAutoDiscoveryEnabled(ui->autoDiscoveryCheckBox->isChecked());
    settings->setCameraSettingsOptimizationEnabled(ui->autoSettingsCheckBox->isChecked());
}

