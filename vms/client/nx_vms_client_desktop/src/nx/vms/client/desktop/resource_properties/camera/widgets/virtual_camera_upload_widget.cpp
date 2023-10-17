// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ui_virtual_camera_upload_widget.h"

#include <QtCore/QFileInfo>

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_properties/camera/flux/camera_settings_dialog_state.h>
#include <nx/vms/client/desktop/resource_properties/camera/flux/camera_settings_dialog_store.h>

#include "virtual_camera_upload_widget.h"

namespace nx::vms::client::desktop {

VirtualCameraUploadWidget::VirtualCameraUploadWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::VirtualCameraUploadWidget)
{
    ui->setupUi(this);

    connect(ui->uploadFileButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(menu::UploadVirtualCameraFileAction); });

    connect(ui->uploadFolderButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(menu::UploadVirtualCameraFolderAction); });

    connect(ui->cancelButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(menu::CancelVirtualCameraUploadsAction); });
}

VirtualCameraUploadWidget::~VirtualCameraUploadWidget()
{
}

void VirtualCameraUploadWidget::setStore(CameraSettingsDialogStore* store)
{
    m_storeConnections = {};

    NX_ASSERT(store);
    if (!store)
        return;

    m_storeConnections << connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &VirtualCameraUploadWidget::loadState);
}

void VirtualCameraUploadWidget::loadState(const CameraSettingsDialogState& state)
{
    bool progressVisible = false;
    const auto& virtualCameraState = state.singleVirtualCameraState;

    switch (virtualCameraState.status)
    {
        case VirtualCameraState::Unlocked:
            ui->stackedWidget->setCurrentWidget(ui->controlsPage);
            break;

        case VirtualCameraState::LockedByOtherClient:
        {
            ui->messageLabel->setText(state.virtualCameraUploaderName.isEmpty()
                ? tr("Another user is currently uploading footage to this camera.")
                : tr("User %1 is currently uploading footage to this camera.")
                    .arg(QString("<b>%1</b>").arg(state.virtualCameraUploaderName)));

            ui->stackedWidget->setCurrentWidget(ui->messagePage);
            break;
        }

        case VirtualCameraState::Locked:
        case VirtualCameraState::Uploading:
        case VirtualCameraState::Consuming:
        {
            const auto calculateFileName =
                [](const VirtualCameraState& state)
                {
                    return QFileInfo(state.currentFile().path).fileName();
                };

            const auto queueMessage = QString(" %1\t%p%").arg((virtualCameraState.queue.size() > 1
                ? tr("(%1 of %2)", "Uploaded and total number of files will be substituted")
                    .arg(virtualCameraState.currentIndex + 1)
                    .arg(virtualCameraState.queue.size())
                : QString()));

            ui->uploadProgressBar->setFormat(virtualCameraState.status == VirtualCameraState::Consuming
                ? tr("Finalizing %1...", "Filename will be substituted")
                    .arg(calculateFileName(virtualCameraState)) + queueMessage
                : tr("Uploading %1...", "Filename will be substituted")
                    .arg(calculateFileName(virtualCameraState)) + queueMessage);

            ui->cancelButton->setEnabled(virtualCameraState.isCancellable());
            ui->uploadProgressBar->setValue(virtualCameraState.progress());
            ui->stackedWidget->setCurrentWidget(ui->controlsPage);
            progressVisible = true;
            break;
        }

        default:
            NX_ASSERT(false, "Should never get here");
            ui->stackedWidget->setCurrentWidget(ui->messagePage);
            ui->messageLabel->setText(QString());
            break;
    }

    ui->controlsPage->setEnabled(!progressVisible);
    ui->progressPanel->setVisible(progressVisible);
    ui->line->setVisible(progressVisible);
}

} // namespace nx::vms::client::desktop
