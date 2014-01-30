#include "camera_management_widget.h"
#include "ui_camera_management_widget.h"

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

}

void QnCameraManagementWidget::submitToSettings() {

}
