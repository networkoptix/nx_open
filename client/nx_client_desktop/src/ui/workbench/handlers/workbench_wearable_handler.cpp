#include "workbench_wearable_handler.h"

#include <QtWidgets/QAction>

#include <nx/utils/string.h>
#include <utils/common/guarded_callback.h>
#include <common/common_module.h>
#include <camera/camera_data_manager.h>
#include <camera/loaders/caching_camera_data_loader.h>

#include <api/server_rest_connection.h>
#include <api/model/wearable_camera_reply.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/dialogs/new_wearable_camera_dialog.h>
#include <ui/dialogs/common/file_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_state_manager.h>

#include <client/client_module.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/messages/resources_messages.h>
#include <nx/client/desktop/utils/wearable_manager.h>
#include <nx/client/desktop/utils/wearable_state.h>
#include <nx/client/desktop/utils/wearable_payload.h>

using namespace nx::client::desktop;
using namespace nx::client::desktop::ui;
using namespace nx::utils;

namespace {
const int kMaxLinesInExtendedErrorMessage = 10;
const QStringList kVideoExtensions = {
    lit("*.avi"), lit("*.mkv"), lit("*.mp4"), lit("*.mov"), lit("*.ts"), lit("*.m2ts"),
    lit("*.mpeg"), lit("*.mpg"), lit("*.flv"), lit("*.wmv"), lit("*.3gp")};
}

class QnWearableSessionDelegate:
    public QObject,
    public QnSessionAwareDelegate
{
public:
    QnWearableSessionDelegate(QObject* parent) :
        QObject(parent),
        QnSessionAwareDelegate(parent)
    {
    }

    virtual bool tryClose(bool force) override
    {
        if (!force)
        {
            QnResourceList resources;
            for (const WearableState& state : qnClientModule->wearableManager()->runningUploads())
            {
                QnResourcePtr resource = resourcePool()->getResourceById(state.cameraId);
                if (resource)
                    resources.push_back(resource);
            }

            if (!messages::Resources::stopWearableUploads(mainWindow(), resources))
                return false;
        }

        qnClientModule->wearableManager()->cancelAllUploads();
        return true;
    }

    virtual void forcedUpdate() override
    {
        qnClientModule->wearableManager()->setCurrentUser(context()->user());
    }
};

QnWorkbenchWearableHandler::QnWorkbenchWearableHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    using namespace ui::action;

    new QnWearableSessionDelegate(this);

    connect(action(NewWearableCameraAction), &QAction::triggered, this,
        &QnWorkbenchWearableHandler::at_newWearableCameraAction_triggered);
    connect(action(UploadWearableCameraFileAction), &QAction::triggered, this,
        &QnWorkbenchWearableHandler::at_uploadWearableCameraFileAction_triggered);
    connect(action(UploadWearableCameraFolderAction), &QAction::triggered, this,
        &QnWorkbenchWearableHandler::at_uploadWearableCameraFolderAction_triggered);
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnWorkbenchWearableHandler::at_resourcePool_resourceAdded);
    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnWorkbenchWearableHandler::at_context_userChanged);
    connect(qnClientModule->wearableManager(), &WearableManager::stateChanged, this,
        &QnWorkbenchWearableHandler::at_wearableManager_stateChanged);
}

QnWorkbenchWearableHandler::~QnWorkbenchWearableHandler()
{
}

void QnWorkbenchWearableHandler::maybeOpenCurrentSettings()
{
    if (m_currentCameraUuid.isNull())
        return;

    auto camera = resourcePool()->getResourceById<QnSecurityCamResource>(m_currentCameraUuid);
    if (!camera)
        return;

    m_currentCameraUuid = QnUuid();
    menu()->trigger(ui::action::CameraSettingsAction, camera);
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

    auto callback = guarded(this,
        [this, server](bool success, rest::Handle, const QnJsonRestResult& reply)
        {
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
        });

    server->restConnection()->addWearableCamera(name, callback, thread());
}

void QnWorkbenchWearableHandler::at_uploadWearableCameraFileAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnSecurityCamResourcePtr camera = parameters.resource().dynamicCast<QnSecurityCamResource>();
    if (!camera || !camera->getParentServer())
        return;

    QStringList filters;
    filters << tr("Video (%1)").arg(kVideoExtensions.join(L' '));
    filters << tr("All files (*.*)");

    QStringList paths = QnFileDialog::getOpenFileNames(mainWindow(),
        tr("Open Wearable Camera Recordings..."),
        QString(),
        filters.join(lit(";;")),
        /*selectedFilter*/ nullptr,
        QnCustomFileDialog::fileDialogOptions()
    );
    if (paths.isEmpty())
        return;

    // TODO: #wearable requested by rvasilenko as copypaste from totalcmd doesn't work without
    // this line. Maybe move directly to QnFileDialog?
    for(QString& path: paths)
        path = path.trimmed();

    qnClientModule->wearableManager()->prepareUploads(camera, paths, this,
        [this, camera](const WearableUpload& upload)
        {
            if(checkFileUpload(upload))
                uploadValidFiles(camera, upload.elements);
        });
}

void QnWorkbenchWearableHandler::at_uploadWearableCameraFolderAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnSecurityCamResourcePtr camera = parameters.resource().dynamicCast<QnSecurityCamResource>();
    if (!camera || !camera->getParentServer())
        return;

    QString path = QnFileDialog::getExistingDirectory(mainWindow(),
        tr("Open Wearable Camera Recordings..."),
        QString(),
        QnCustomFileDialog::directoryDialogOptions()
    );
    if (path.isEmpty())
        return;

    QStringList files = QDir(path).entryList(kVideoExtensions);
    if (files.empty())
    {
        QnMessageBox::critical(mainWindow(),
            tr("No video files found in \"%1\"").arg(path));
        return;
    }

    for (QString& file : files)
        file = path + QDir::separator() + file;

    qnClientModule->wearableManager()->prepareUploads(camera, files, this,
        [this, path, camera](const WearableUpload& upload)
        {
            if (checkFolderUpload(path, upload))
                uploadValidFiles(camera, upload.elements);
        });
}

bool QnWorkbenchWearableHandler::checkFileUpload(const WearableUpload& upload)
{
    int count = upload.elements.size();

    if (upload.allHaveStatus(WearablePayload::UnsupportedFormat))
    {
        QnMessageBox::critical(mainWindow(),
            tr("Selected file format(s) are not supported", 0, count),
            tr("Only video files are supported."));
        return false;
    }

    if (upload.allHaveStatus(WearablePayload::NoTimestamp))
    {
        QnMessageBox::critical(mainWindow(),
            tr("Selected file(s) do not have timestamp(s)", 0, count),
            tr("Only video files with correct timestamp are supported."));
        return false;
    }

    if (upload.allHaveStatus(WearablePayload::ChunksTakenByFileInQueue))
    {
        QnMessageBox::critical(mainWindow(),
            tr("Selected file(s) cover periods for which videos are already being uploaded", 0, count),
            tr("You can upload these file(s) to a different instance of a Wearable Camera.", 0, count));
        return false;
    }

    if (upload.allHaveStatus(WearablePayload::ChunksTakenOnServer))
    {
        QnMessageBox::critical(mainWindow(),
            tr("Selected file(s) cover periods for which videos have already been uploaded", 0, count),
            tr("You can upload these file(s) to a different instance of a Wearable Camera.", 0, count));
        return false;
    }

    if (upload.allHaveStatus(WearablePayload::NoSpaceOnServer))
    {
        showNoSpaceOnServerWarning(upload);
        return false;
    }

    if (upload.allHaveStatus(WearablePayload::ServerError))
        return false; //< Ignore it as the user is likely already seeing "reconnecting to server" dialog.

    if (!upload.allHaveStatus(WearablePayload::Valid))
    {
        QString extendedMessage;
        int lines = 0;
        for (const WearablePayload& payload : upload.elements)
        {
            QString line = calculateExtendedErrorMessage(payload);
            if (!line.isEmpty())
            {
                extendedMessage += line + lit("\n");
                lines++;
            }

            if (lines == kMaxLinesInExtendedErrorMessage)
                break;
        }

        if (!upload.someHaveStatus(WearablePayload::Valid))
        {
            QnMessageBox::critical(mainWindow(),
                tr("Selected file(s) will not be uploaded", 0, count),
                extendedMessage);
            return false;
        }
        else
        {
            QDialogButtonBox::StandardButton button = QnMessageBox::critical(mainWindow(),
                tr("Some file(s) will not be uploaded", 0, count),
                extendedMessage,
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            if (button == QDialogButtonBox::Cancel)
                return false;
        }
    }

    return true;
}

bool QnWorkbenchWearableHandler::checkFolderUpload(const QString& path, const WearableUpload& upload)
{
    if (upload.someHaveStatus(WearablePayload::NoSpaceOnServer))
    {
        showNoSpaceOnServerWarning(upload);
        return false;
    }

    if (!upload.someHaveStatus(WearablePayload::Valid))
    {
        QnMessageBox::critical(mainWindow(),
            tr("There is no new files to upload in \"%1\"").arg(path));
        return false;
    }

    return true;
}

void QnWorkbenchWearableHandler::showNoSpaceOnServerWarning(const WearableUpload& upload)
{
    QnMessageBox::critical(mainWindow(),
        tr("Not enough space on server storage"),
        tr("File(s) size - %1\nFree space - %2", 0, upload.elements.size())
            .arg(formatFileSize(upload.spaceRequested))
            .arg(formatFileSize(upload.spaceAvailable)));
}

void QnWorkbenchWearableHandler::uploadValidFiles(
    const QnSecurityCamResourcePtr& camera,
    const WearablePayloadList& payloads)
{
    WearablePayloadList validPayloads;
    for (const WearablePayload& payload : payloads)
        if (payload.status == WearablePayload::Valid)
            validPayloads.push_back(payload);

    if (!qnClientModule->wearableManager()->addUpload(camera, validPayloads))
    {
        WearableState state = qnClientModule->wearableManager()->state(camera);
        NX_ASSERT(state.status == WearableState::LockedByOtherClient);

        QString message;
        QnResourcePtr resource = resourcePool()->getResourceById(state.lockUserId);
        if (resource)
            message = tr("Could not start upload as user \"%1\" is currently uploading footage to this camera.").arg(resource->getName());
        else
            message = tr("Could not start upload as another user is currently uploading footage to this camera.");

        QnMessageBox::critical(mainWindow(), message);
    }
}

void QnWorkbenchWearableHandler::at_resourcePool_resourceAdded(const QnResourcePtr &resource)
{
    if (resource->getId() == m_currentCameraUuid)
        maybeOpenCurrentSettings();
}

void QnWorkbenchWearableHandler::at_context_userChanged()
{
    qnClientModule->wearableManager()->setCurrentUser(context()->user());
}

void QnWorkbenchWearableHandler::at_wearableManager_stateChanged(const WearableState& state)
{
    if (state.consumeProgress != 100 || state.status != WearableState::Consuming)
        return;

    QnSecurityCamResourcePtr camera =
        resourcePool()->getResourceById<QnSecurityCamResource>(state.cameraId);
    if (!camera)
        return;

    qnClientModule->cameraDataManager()
        ->loader(camera, /*create=*/true)
        ->load(/*forced=*/true);
}

QString QnWorkbenchWearableHandler::calculateExtendedErrorMessage(const WearablePayload& upload)
{
    QString fileName = QFileInfo(upload.path).fileName();

    switch (upload.status)
    {
    case WearablePayload::UnsupportedFormat:
        return tr("File format of \"%1\" is not supported.").arg(fileName);
    case WearablePayload::NoTimestamp:
        return tr("File \"%1\" does not have timestamp.").arg(fileName);
    case WearablePayload::ChunksTakenByFileInQueue:
        return tr("File \"%1\" covers period(s) that video is already being uploaded for.").arg(fileName);
    case WearablePayload::ChunksTakenOnServer:
        return tr("File \"%1\" covers period(s) that video has already been uploaded for.").arg(fileName);
    case WearablePayload::NoSpaceOnServer:
        return tr("There is no space on server for file \"%1\".").arg(fileName);
    default:
        return QString();
    }
}
