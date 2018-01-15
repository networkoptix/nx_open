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
#include <api/server_rest_connection.h>

#include <client/client_module.h>
#include <client/client_upload_manager.h>
#include <client/client_wearable_manager.h>

namespace {
    /* Using TTL of 10 mins for uploads. This shall be enough even for the most extreme cases.
     * Also note that undershooting is not a problem here as a file that's currently open won't be
     * deleted. */
    const qint64 kDefaultUploadTtl = 1000 * 60 * 10;
}

QnWorkbenchWearableHandler::QnWorkbenchWearableHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    using namespace nx::client::desktop::ui::action;

    connect(action(NewWearableCameraAction), &QAction::triggered, this, &QnWorkbenchWearableHandler::at_newWearableCameraAction_triggered);
    connect(action(UploadWearableCameraFileAction), &QAction::triggered, this, &QnWorkbenchWearableHandler::at_uploadWearableCameraFileAction_triggered);
    connect(resourcePool(), &QnResourcePool::resourceAdded, this, &QnWorkbenchWearableHandler::at_resourcePool_resourceAdded);
}

QnWorkbenchWearableHandler::~QnWorkbenchWearableHandler()
{
}

qreal QnWorkbenchWearableHandler::calculateProgress(const QnFileUpload& upload, bool processed) {
    if (processed)
        return 1.0;

    switch (upload.status) {
    case QnFileUpload::CreatingUpload:
        return 0.1;
    case QnFileUpload::Uploading:
        return 0.1 + 0.8 * upload.uploaded / upload.size;
    case QnFileUpload::Checking:
    case QnFileUpload::Done:
        return 0.9;
    default:
        return 0.0;
    }
}

void QnWorkbenchWearableHandler::maybeOpenCurrentSettings()
{
    using namespace nx::client::desktop::ui::action;

    if (m_currentCameraUuid.isNull())
        return;

    QnSecurityCamResourcePtr camera = resourcePool()->getResourceById<QnSecurityCamResource>(m_currentCameraUuid);
    if (!camera)
        return;

    m_currentCameraUuid = QnUuid();

    menu()->trigger(CameraSettingsAction, camera);
}

void QnWorkbenchWearableHandler::at_newWearableCameraAction_triggered()
{
    if (!commonModule()->currentServer())
        return;

    std::unique_ptr<QnNewWearableCameraDialog> dialog = std::make_unique<QnNewWearableCameraDialog>(mainWindow());
    if (dialog->exec() != QDialog::Accepted)
        return;

    QString name = dialog->name();
    QnMediaServerResourcePtr server = dialog->server();

    QPointer<QObject> guard(this);
    auto callback = [this, guard, server](bool success, rest::Handle, const QnJsonRestResult& reply)
    {
        if (!guard)
            return;

        if (!success)
        {
            QnMessageBox::critical(
                mainWindow(),
                tr("Could not add wearable camera to server \"%1\".").arg(server->getName())
            );
            return;
        }

        m_currentCameraUuid = reply.deserialized<QnWearableCameraReply>().id;
        maybeOpenCurrentSettings();
    };

    server->restConnection()->addWearableCameraAsync(name, callback, thread());
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
    if (fileName.isEmpty())
        return;

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

    QnFileUpload upload = qnClientModule->uploadManager()->addUpload(
        server,
        fileName,
        kDefaultUploadTtl,
        this,
        [this](const QnFileUpload& upload) { at_upload_progress(upload); }
    );

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

void QnWorkbenchWearableHandler::at_upload_progress(const QnFileUpload& upload)
{
    FootageInfo info = m_infoByUploadId.value(upload.id);

    qnClientModule->wearableManager()->tehProgress(calculateProgress(upload, false));

    if (upload.status == QnFileUpload::Error)
    {
        m_infoByUploadId.remove(upload.id);
        return;
    }

    if (upload.status == QnFileUpload::Done)
    {
        /* Check if it's our download. */
        if (!info.camera)
            return;

        QPointer<QObject> guard(this);
        auto callback = [this, guard, upload](bool success, rest::Handle, const rest::ServerConnection::EmptyResponseType&)
        {
            if (!guard)
                return;

            qnClientModule->wearableManager()->tehProgress(calculateProgress(upload, true));

            // yeah!
            int a = 10;
        };

        QnMediaServerResourcePtr server = info.camera->getParentServer();
        server->restConnection()->consumeWearableCameraFileAsync(
            info.camera,
            upload.id,
            info.startTimeMs,
            callback,
            thread());
    }
}

