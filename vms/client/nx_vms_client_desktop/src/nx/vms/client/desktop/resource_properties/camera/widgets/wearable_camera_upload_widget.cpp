#include "wearable_camera_upload_widget.h"
#include "ui_wearable_camera_upload_widget.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <QtCore/QFileInfo>

#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

WearableCameraUploadWidget::WearableCameraUploadWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::WearableCameraUploadWidget)
{
    ui->setupUi(this);

    connect(ui->uploadFileButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(ui::action::UploadWearableCameraFileAction); });

    connect(ui->uploadFolderButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(ui::action::UploadWearableCameraFolderAction); });

    connect(ui->cancelButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(ui::action::CancelWearableCameraUploadsAction); });
}

WearableCameraUploadWidget::~WearableCameraUploadWidget()
{
}

void WearableCameraUploadWidget::setStore(CameraSettingsDialogStore* store)
{
    m_storeConnections = {};

    NX_ASSERT(store);
    if (!store)
        return;

    m_storeConnections << connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &WearableCameraUploadWidget::loadState);
}

void WearableCameraUploadWidget::loadState(const CameraSettingsDialogState& state)
{
    bool progressVisible = false;
    const auto& wearableState = state.singleWearableState;

    switch (wearableState.status)
    {
        case WearableState::Unlocked:
            ui->stackedWidget->setCurrentWidget(ui->controlsPage);
            break;

        case WearableState::LockedByOtherClient:
        {
            ui->messageLabel->setText(state.wearableUploaderName.isEmpty()
                ? tr("Another user is currently uploading footage to this camera.")
                : tr("User %1 is currently uploading footage to this camera.")
                    .arg(lit("<b>%1</b>").arg(state.wearableUploaderName)));

            ui->stackedWidget->setCurrentWidget(ui->messagePage);
            break;
        }

        case WearableState::Locked:
        case WearableState::Uploading:
        case WearableState::Consuming:
        {
            const auto calculateFileName =
                [this](const WearableState& state)
                {
                    return QFileInfo(state.currentFile().path).fileName();
                };

            const auto queueMessage = lit(" %1\t%p%").arg((wearableState.queue.size() > 1
                ? tr("(%1 of %2)").arg(wearableState.currentIndex + 1).arg(wearableState.queue.size())
                : QString()));

            ui->uploadProgressBar->setFormat(wearableState.status == WearableState::Consuming
                ? tr("Finalizing %1...").arg(calculateFileName(wearableState)) + queueMessage
                : tr("Uploading %1...").arg(calculateFileName(wearableState)) + queueMessage);

            ui->cancelButton->setEnabled(wearableState.isCancellable());
            ui->uploadProgressBar->setValue(wearableState.progress());
            ui->stackedWidget->setCurrentWidget(ui->controlsPage);
            progressVisible = true;
            break;
        }

        default:
            NX_ASSERT(false, Q_FUNC_INFO, "Should never get here");
            ui->stackedWidget->setCurrentWidget(ui->messagePage);
            ui->messageLabel->setText(QString());
            break;
    }

    ui->controlsPage->setEnabled(!progressVisible);
    ui->progressPanel->setVisible(progressVisible);
    ui->line->setVisible(progressVisible);
}

} // namespace nx::vms::client::desktop
