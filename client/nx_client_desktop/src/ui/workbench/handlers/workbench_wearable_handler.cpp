#include "workbench_wearable_handler.h"

#include <QtWidgets/QAction>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <api/model/wearable_camera_reply.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/dialogs/new_wearable_camera_dialog.h>
#include <ui/dialogs/common/file_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/common/read_only.h>

QnWorkbenchWearableHandler::QnWorkbenchWearableHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    using namespace nx::client::desktop::ui::action;

    connect(action(NewWearableCameraAction), &QAction::triggered, this, &QnWorkbenchWearableHandler::at_newWearableCameraAction_triggered);
    connect(action(UploadWearableCameraFileAction), &QAction::triggered, this, &QnWorkbenchWearableHandler::at_uploadWearableCameraFileAction_triggered);
    connect(resourcePool(), &QnResourcePool::resourceAdded, this, &QnWorkbenchWearableHandler::at_resourcePool_resourceAdded);
}

QnWorkbenchWearableHandler::~QnWorkbenchWearableHandler() {

}

void QnWorkbenchWearableHandler::maybeOpenSettings() {
    using namespace nx::client::desktop::ui::action;

    if (!m_dialog || m_uuid.isNull())
        return;

    QnSecurityCamResourcePtr camera = resourcePool()->getResourceById<QnSecurityCamResource>(m_uuid);
    if (!camera)
        return;

    m_dialog->deleteLater();
    m_uuid = QnUuid();

    menu()->trigger(UploadWearableCameraFileAction, camera);
}

void QnWorkbenchWearableHandler::at_newWearableCameraAction_triggered() {
    if (!commonModule()->currentServer())
        return; /* Should never get here, really. */

    if (m_dialog)
        return; /* And here... */

    m_dialog = new QnNewWearableCameraDialog(mainWindow());

    /* Delete dialog when rejected, this will also handle disconnects from server. */
    connect(m_dialog, &QDialog::rejected, m_dialog, &QDialog::deleteLater);
    if (m_dialog->exec() != QDialog::Accepted)
        return;
    setReadOnly(m_dialog.data(), true); /* Wait for signals to come through. */

    QString name = m_dialog->name();
    QnMediaServerResourcePtr server = m_dialog->server();
    server->apiConnection()->addWearableCameraAsync(name, this, SLOT(at_addWearableCameraAsync_finished(int, const QnWearableCameraReply &, int)));
}

void QnWorkbenchWearableHandler::at_addWearableCameraAsync_finished(int status, const QnWearableCameraReply &reply, int /*handle*/) {
    if (status != 0) {
        QnMessageBox messageBox(
            QnMessageBoxIcon::Critical,
            tr("Could not add wearable camera to server \"%1\".").arg(m_dialog->server()->getName()), 
            QString(), 
            QDialogButtonBox::Ok, 
            QDialogButtonBox::Ok,
            mainWindow()
        );
        messageBox.exec();

        m_dialog->deleteLater();
        return;
    }

    m_uuid = reply.id;
    maybeOpenSettings();
}

void QnWorkbenchWearableHandler::at_resourcePool_resourceAdded(const QnResourcePtr &) {
    maybeOpenSettings();
}

void QnWorkbenchWearableHandler::at_uploadWearableCameraFileAction_triggered() {
    QnSecurityCamResourcePtr camera = menu()->currentParameters(sender()).resource().dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;
    QnMediaServerResourcePtr server = camera->getParentServer();

    QString fileName = QnFileDialog::getOpenFileName(
        mainWindow(),
        tr("Open Wearable Camera Recording..."),
        QString(),
        tr("All files (*.*)"),
        0,
        QnCustomFileDialog::fileDialogOptions());

    QFile file(fileName);
    file.open(QIODevice::ReadOnly);
    QByteArray data = file.readAll();
    file.close();

    QFileInfo info(fileName);
    qint64 startTimeMs = info.created().toMSecsSinceEpoch();

    server->apiConnection()->uploadWearableCameraFileAsync(camera, data, startTimeMs, this, SLOT(at_uploadWearableCameraFileAsync_finished(int, int)));
}

void QnWorkbenchWearableHandler::at_uploadWearableCameraFileAsync_finished(int status, int handle) {

}
