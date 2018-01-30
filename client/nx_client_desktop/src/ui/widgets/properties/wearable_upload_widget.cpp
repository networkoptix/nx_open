#include "wearable_upload_widget.h"
#include "ui_wearable_upload_widget.h"

#include <QtCore/QTimer>

#include <client/client_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/utils/wearable_manager.h>

using namespace nx::client::desktop;

namespace {
const int kStatePollPeriodMSec = 1000 * 2;
}

QnWearableUploadWidget::QnWearableUploadWidget(QWidget* parent):
    QWidget(parent),
    QnWorkbenchContextAware(parent, InitializationMode::lazy),
    ui(new Ui::WearableUploadWidget)
{
    ui->setupUi(this);

    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &QnWearableUploadWidget::maybePollState);
    timer->start(kStatePollPeriodMSec);

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

    ui->uploadFolderButton->hide(); // TODO: #wearable
}

QnWearableUploadWidget::~QnWearableUploadWidget()
{
}

void QnWearableUploadWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (m_camera == camera)
        return;

    m_camera = camera;

    updateFromState(qnClientModule->wearableManager()->state(camera));
    maybePollState();
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

void QnWearableUploadWidget::maybePollState()
{
    if (!isVisible())
        return;

    if (!m_camera)
        return;

    WearableState state = qnClientModule->wearableManager()->state(m_camera);
    if(state.status == WearableState::Unlocked || state.status == WearableState::LockedByOtherClient)
        qnClientModule->wearableManager()->updateState(m_camera);
}