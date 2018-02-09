#include "wearable_progress_widget.h"
#include "ui_wearable_progress_widget.h"

#include <QtCore/QFileInfo>

#include <client/client_module.h>

#include <core/resource/camera_resource.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/utils/wearable_manager.h>

using namespace nx::client::desktop;

QnWearableProgressWidget::QnWearableProgressWidget(QWidget* parent):
    QWidget(parent),
    QnWorkbenchContextAware(parent, true),
    ui(new Ui::WearableProgressWidget)
{
    ui->setupUi(this);

    connect(qnClientModule->wearableManager(), &WearableManager::stateChanged, this,
        [this](const WearableState& state)
        {
            if (m_camera && state.cameraId == m_camera->getId())
                updateFromState(state);
        });

    connect(ui->cancelButton, &QPushButton::clicked, this,
        [this]
        {
            if (m_camera)
                qnClientModule->wearableManager()->cancelUploads(m_camera);
        });
}

QnWearableProgressWidget::~QnWearableProgressWidget()
{
}

void QnWearableProgressWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (m_camera == camera)
        return;

    m_camera = camera;

    updateFromState(qnClientModule->wearableManager()->state(camera));
}

QnVirtualCameraResourcePtr QnWearableProgressWidget::camera() const
{
    return m_camera;
}

bool QnWearableProgressWidget::isActive()
{
    return m_active;
}

void QnWearableProgressWidget::setActive(bool active)
{
    if (m_active == active)
        return;

    m_active = active;

    emit activeChanged();
}

void QnWearableProgressWidget::updateFromState(const WearableState& state)
{
    setActive(calculateActive(state));
    ui->cancelButton->setEnabled(calculateCancelable(state));
    ui->uploadProgressBar->setValue(state.progress());
    ui->uploadProgressBar->setFormat(calculateMessage(state));
}

bool QnWearableProgressWidget::calculateActive(const WearableState& state)
{
    switch (state.status)
    {
    case WearableState::Unlocked:
    case WearableState::LockedByOtherClient:
        return false;
    case WearableState::Locked:
    case WearableState::Uploading:
    case WearableState::Consuming:
        return true;
    default:
        return false;
    }
}

bool QnWearableProgressWidget::calculateCancelable(const WearableState& state)
{
    if (state.currentIndex + 1 == state.queue.size())
        return state.status != WearableState::Consuming;
    if (state.currentIndex >= state.queue.size())
        return false;
    return true;
}

QString QnWearableProgressWidget::calculateMessage(const WearableState& state)
{
    switch (state.status)
    {
    case WearableState::Unlocked:
    case WearableState::LockedByOtherClient:
        return QString();
    case WearableState::Locked:
    case WearableState::Uploading:
        return tr("Uploading %1... %2\t%p%")
            .arg(calculateFileName(state))
            .arg(calculateQueueMessage(state));
    case WearableState::Consuming:
        return tr("Finalizing %1... %2\t%p%")
            .arg(calculateFileName(state))
            .arg(calculateQueueMessage(state));;
    default:
        return QString();
    }
}

QString QnWearableProgressWidget::calculateQueueMessage(const WearableState& state)
{
    int left = state.queue.size() - (state.currentIndex + 1);

    if (left <= 0)
        return QString();
    else
        return tr("(%n more file(s) in queue)", "", left);
}

QString QnWearableProgressWidget::calculateFileName(const WearableState& state)
{
    return QFileInfo(state.currentFile().path).fileName();
}

