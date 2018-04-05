#include "wearable_progress_widget.h"
#include "ui_wearable_progress_widget.h"

#include <QtCore/QFileInfo>

#include <core/resource/camera_resource.h>

#include <client/client_module.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/utils/wearable_manager.h>
#include <nx/client/desktop/utils/wearable_state.h>
#include <ui/dialogs/common/message_box.h>

using namespace nx::client::desktop;

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
        &QnWearableProgressWidget::at_cancelButton_clicked);
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

void QnWearableProgressWidget::at_cancelButton_clicked()
{
    if (!m_camera)
        return;

    WearableState state = qnClientModule->wearableManager()->state(m_camera);

    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Stop uploading?"), tr("Already uploaded files will be kept."),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, this);
    dialog.addCustomButton(QnMessageBoxCustomButton::Stop,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    if (dialog.exec() == QDialogButtonBox::Cancel)
        return;

    qnClientModule->wearableManager()->cancelUploads(m_camera);
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

