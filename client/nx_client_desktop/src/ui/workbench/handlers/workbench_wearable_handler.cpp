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
#include <plugins/resource/avi/avi_archive_delegate.h>
#include <plugins/resource/avi/avi_resource.h>

#include <client/client_module.h>
#include <client/client_upload_manager.h>

QnWorkbenchWearableHandler::QnWorkbenchWearableHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    using namespace nx::client::desktop::ui::action;

    connect(action(NewWearableCameraAction), &QAction::triggered, this, &QnWorkbenchWearableHandler::at_newWearableCameraAction_triggered);
    connect(action(UploadWearableCameraFileAction), &QAction::triggered, this, &QnWorkbenchWearableHandler::at_uploadWearableCameraFileAction_triggered);
    connect(resourcePool(), &QnResourcePool::resourceAdded, this, &QnWorkbenchWearableHandler::at_resourcePool_resourceAdded);
    connect(qnClientModule->uploadManager(), &QnClientUploadManager::progress, this, &QnWorkbenchWearableHandler::at_uploadManager_progress);
}

QnWorkbenchWearableHandler::~QnWorkbenchWearableHandler()
{
}

void QnWorkbenchWearableHandler::maybeOpenCurrentSettings()
{
    using namespace nx::client::desktop::ui::action;

    if (!m_dialog || m_currentCameraUuid.isNull())
        return;

    QnSecurityCamResourcePtr camera = resourcePool()->getResourceById<QnSecurityCamResource>(m_currentCameraUuid);
    if (!camera)
        return;

    m_dialog->deleteLater();
    m_currentCameraUuid = QnUuid();

    menu()->trigger(UploadWearableCameraFileAction, camera);
}

void QnWorkbenchWearableHandler::at_newWearableCameraAction_triggered()
{
    if (!commonModule()->currentServer())
        return;

    if (m_dialog)
        return;

    m_dialog = new QnNewWearableCameraDialog(mainWindow());

    /* Delete dialog when rejected, this will also handle disconnects from server. */
    connect(m_dialog, &QDialog::rejected, m_dialog, &QDialog::deleteLater);
    if (m_dialog->exec() != QDialog::Accepted)
        return;

    /* Wait for signals to come through. */
    setReadOnly(m_dialog.data(), true);

    QString name = m_dialog->name();
    QnMediaServerResourcePtr server = m_dialog->server();
    server->apiConnection()->addWearableCameraAsync(
        name,
        this,
        SLOT(at_addWearableCameraAsync_finished(int, const QnWearableCameraReply &, int)));
}

void QnWorkbenchWearableHandler::at_addWearableCameraAsync_finished(int status, const QnWearableCameraReply &reply, int)
{
    if (status != 0)
    {
        QnMessageBox::critical(
            mainWindow(),
            tr("Could not add wearable camera to server \"%1\".").arg(m_dialog->server()->getName())
        );

        m_dialog->deleteLater();
        return;
    }

    m_currentCameraUuid = reply.id;
    maybeOpenCurrentSettings();
}

void QnWorkbenchWearableHandler::at_resourcePool_resourceAdded(const QnResourcePtr &)
{
    maybeOpenCurrentSettings();
}

void QnWorkbenchWearableHandler::at_uploadWearableCameraFileAction_triggered()
{
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
        QnCustomFileDialog::fileDialogOptions()
    );

    QnAviResourcePtr resource(new QnAviResource(fileName));
    QnAviArchiveDelegatePtr delegate(resource->createArchiveDelegate());
    bool opened = delegate->open(resource);

    if (!opened || !delegate->hasVideo() || resource->hasFlags(Qn::still_image))
    {
        QnMessageBox::critical(
            mainWindow(),
            tr("File \"%1\" is not a video file.").arg(fileName)
        );
        return;
    }

    qint64 startTimeMs = delegate->startTime() / 1000;
    qint64 durationMs = (delegate->endTime() - delegate->startTime()) / 1000;

    if (startTimeMs == 0)
    {
        QString startTimeString = QLatin1String(delegate->getTagValue("creation_time"));
        QDateTime startDateTime = QDateTime::fromString(startTimeString, Qt::ISODate);
        if (startDateTime.isValid())
            startTimeMs = startDateTime.toMSecsSinceEpoch();
    }

    if (startTimeMs == 0)
        startTimeMs = QFileInfo(fileName).created().toMSecsSinceEpoch();

    QnFileUpload upload = qnClientModule->uploadManager()->addUpload(server, fileName);

    if (upload.status == QnFileUpload::Error)
    {
        QnMessageBox::critical(
            mainWindow(),
            upload.errorMessage
        );
        return;
    }

    FootageInfo info;
    info.camera = camera;
    info.startTimeMs = startTimeMs;
    m_infoByUploadId[upload.id] = info;
}

void QnWorkbenchWearableHandler::at_uploadManager_progress(const QnFileUpload& upload)
{
    if (upload.status == QnFileUpload::Error)
    {
        m_infoByUploadId.remove(upload.id);
        return;
    }

    if (upload.status == QnFileUpload::Done)
    {
        /* Check if it's our download. */
        FootageInfo info = m_infoByUploadId.value(upload.id);
        if (!info.camera)
            return;

        QnMediaServerResourcePtr server = info.camera->getParentServer();
        server->apiConnection()->consumeWearableCameraFileAsync(
            info.camera,
            upload.id,
            info.startTimeMs,
            this,
            SLOT(at_processWearableCameraFileAsync_finished(int, int)));
    }
}

void QnWorkbenchWearableHandler::at_processWearableCameraFileAsync_finished(int status, int handle)
{
    // yeah!
    int a = 10;
}
