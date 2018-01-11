#include "wearable_upload_widget.h"
#include "ui_wearable_upload_widget.h"

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <core/resource/camera_resource.h>


QnWearableUploadWidget::QnWearableUploadWidget(QWidget *parent)
    : QWidget(parent)
    , QnWorkbenchContextAware(parent, true)
    , ui(new Ui::WearableUploadWidget)

{
    ui->setupUi(this);

    connect(ui->uploadFileButton, &QPushButton::pressed, this,
        [this]()
        {
            if (m_camera)
                menu()->trigger(nx::client::desktop::ui::action::UploadWearableCameraFileAction, m_camera);
        }
    );
}

QnWearableUploadWidget::~QnWearableUploadWidget()
{

}

void QnWearableUploadWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    m_camera = camera;
}

QnVirtualCameraResourcePtr QnWearableUploadWidget::camera() const
{
    return m_camera;
}
