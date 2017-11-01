#include "camera_password_change_dialog.h"
#include "ui_camera_password_change_dialog.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

QnCameraPasswordChangeDialog::QnCameraPasswordChangeDialog(
    const QnVirtualCameraResourceList& cameras,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::CameraPasswordChangeDialog)
{
    ui->setupUi(this);
    if (cameras.size() > 1)
        ui->resourcesWidget->setResources(cameras);
    else
        ui->resourcesPanel->setVisible(false);

    setResizeToContentsMode(Qt::Vertical);
}

QnCameraPasswordChangeDialog::~QnCameraPasswordChangeDialog()
{
}

