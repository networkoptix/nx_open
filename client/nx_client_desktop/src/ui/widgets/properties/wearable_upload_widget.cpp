#include "wearable_upload_widget.h"
#include "ui_wearable_upload_widget.h"

#include <client/client_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/utils/wearable_manager.h>

using namespace nx::client::desktop;

QnWearableUploadWidget::QnWearableUploadWidget(QWidget* parent):
    QWidget(parent),
    QnWorkbenchContextAware(parent, true),
    ui(new Ui::WearableUploadWidget)
{
    ui->setupUi(this);

    connect(ui->uploadFileButton, &QPushButton::pressed, this,
        [this]()
        {
            if (m_camera)
                menu()->trigger(ui::action::UploadWearableCameraFileAction, m_camera);
        });

    connect(qnClientModule->wearableManager(), &WearableManager::stateChanged, this,
        [this](const WearableState& state)
        {
            if (m_camera && state.cameraId == m_camera->getId())
                updateFromState(state);
        });
}

QnWearableUploadWidget::~QnWearableUploadWidget()
{
}

void QnWearableUploadWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (m_camera == camera)
        return;

    m_camera = camera;

    if (m_camera)
    {
        updateFromState(qnClientModule->wearableManager()->state(camera));
        qnClientModule->wearableManager()->updateState(camera);
    }
    else
    {
        updateFromState(WearableState());
    }
}

QnVirtualCameraResourcePtr QnWearableUploadWidget::camera() const
{
    return m_camera;
}

void QnWearableUploadWidget::updateFromState(const WearableState& state)
{
    bool enabled = calculateEnabled(state);
    ui->uploadFileButton->setEnabled(enabled);
    ui->uploadFolderButton->setEnabled(enabled);
    ui->warningLabel->setText(calculateMessage(state));
}

bool QnWearableUploadWidget::calculateEnabled(const WearableState& state)
{
    return state.status != WearableState::LockedByOtherClient;
}

QString QnWearableUploadWidget::calculateMessage(const WearableState& state)
{
    if (state.status != WearableState::LockedByOtherClient)
        return QString();

    QnResourcePtr user = resourcePool()->getResourceById(state.lockUserId);
    if (user)
        return tr("User \"%1\" is currently uploading footage to this camera.").arg(user->getName());
    else
        return tr("Another user is currently uploading footage to this camera.");
}
