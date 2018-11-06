#include "legacy_wearable_progress_widget.h"
#include "ui_legacy_wearable_progress_widget.h"

#include <QtCore/QFileInfo>

#include <core/resource/camera_resource.h>

#include <client/client_module.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/utils/wearable_manager.h>
#include <nx/vms/client/desktop/utils/wearable_state.h>
#include <ui/dialogs/common/message_box.h>

using namespace nx::vms::client::desktop;

QnWearableProgressWidget::QnWearableProgressWidget(QWidget* parent):
    QWidget(parent),
    QnWorkbenchContextAware(parent, InitializationMode::lazy),
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
        [this]()
        {
            if (m_camera)
                menu()->trigger(ui::action::CancelWearableCameraUploadsAction, m_camera);
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
    ui->cancelButton->setEnabled(state.isCancellable());
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

QString QnWearableProgressWidget::calculateMessage(const WearableState& state)
{
    const QString queueMessage = lit(" %1\t%p%").arg(calculateQueueMessage(state));
    switch (state.status)
    {
        case WearableState::Unlocked:
        case WearableState::LockedByOtherClient:
            return QString();

        case WearableState::Locked:
        case WearableState::Uploading:
            return tr("Uploading %1...").arg(calculateFileName(state)) + queueMessage;

        case WearableState::Consuming:
            return tr("Finalizing %1...").arg(calculateFileName(state)) + queueMessage;

        default:
            return QString();
    }
}

QString QnWearableProgressWidget::calculateQueueMessage(const WearableState& state)
{
    if (state.queue.size() <= 1)
        return QString();

    return tr("(%1 of %2)").arg(state.currentIndex + 1).arg(state.queue.size());
}

QString QnWearableProgressWidget::calculateFileName(const WearableState& state)
{
    return QFileInfo(state.currentFile().path).fileName();
}

