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
                    tr("Failed to add wearable camera")
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

    QStringList paths = QFileDialog::getOpenFileNames(mainWindow(),
        tr("Open Wearable Camera Recordings..."),
        QString(),
        filters.join(lit(";;")),
        /*selectedFilter*/ nullptr,
        QnCustomFileDialog::fileDialogOptions()
    );
    if (paths.isEmpty())
        return;

    // TODO: #wearable requested by rvasilenko as copypaste from totalcmd doesn't work without
    // this line. Maybe move directly to QFileDialog?
    for(QString& path: paths)
        path = path.trimmed();

    qnClientModule->wearableManager()->prepareUploads(camera, paths, this,
        [this, camera](WearableUpload upload)
        {
            if(fixFileUpload(camera, &upload))
                uploadValidFiles(camera, upload.elements);
        });
}

void QnWorkbenchWearableHandler::at_uploadWearableCameraFolderAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnSecurityCamResourcePtr camera = parameters.resource().dynamicCast<QnSecurityCamResource>();
    if (!camera || !camera->getParentServer())
        return;

    QString path = QFileDialog::getExistingDirectory(mainWindow(),
        tr("Open Wearable Camera Recordings..."),
        QString(),
        QnCustomFileDialog::directoryDialogOptions()
    );
    if (path.isEmpty())
        return;

    QStringList files;
    QDirIterator iterator(path, kVideoExtensions, QDir::NoFilter, QDirIterator::Subdirectories);
    while (iterator.hasNext())
        files.push_back(iterator.next());

    if (files.empty())
    {
        QnMessageBox::warning(mainWindow(),
            tr("No video files found in selected folder"),
            path);
        return;
    }

    qnClientModule->wearableManager()->prepareUploads(camera, files, this,
        [this, path, camera](WearableUpload upload)
        {
            if (fixFolderUpload(path, camera, &upload))
                uploadValidFiles(camera, upload.elements);
        });
}

bool QnWorkbenchWearableHandler::fixFileUpload(
    const QnSecurityCamResourcePtr& camera,
    WearableUpload* upload)
{
    int count = upload->elements.size();

    if (upload->allHaveStatus(WearablePayload::UnsupportedFormat))
    {
        QnMessageBox::warning(mainWindow(),
            tr("Selected file formats are not supported", "", count),
            tr("Use .MKV, .AVI, .MP4 or other video files."));
        return false;
    }

    if (upload->allHaveStatus(WearablePayload::NoTimestamp))
    {
        QnMessageBox::warning(mainWindow(),
            tr("Selected files do not have timestamps", "", count),
            tr("Only video files with correct timestamp are supported."));
        return false;
    }

    if (upload->allHaveStatus(WearablePayload::FootagePastMaxDays))
    {
        qint64 minTime = std::numeric_limits<qint64>::max();
        qint64 maxTime = std::numeric_limits<qint64>::min();
        for (const WearablePayload& payload : upload->elements)
        {
            minTime = std::min(minTime, payload.local.period.startTimeMs);
            maxTime = std::max(maxTime, payload.local.period.endTimeMs());
        }

        QString title = tr("Selected files are too old", "", count);

        int days = camera->maxDays();
        QString extra;
        if (count == 1)
        {
            extra = tr(
                "Selected file was recorded on %1, "
                "but only files that were recorded in the last %n days can be uploaded. "
                "You can change this in camera archive settings.", "", days)
                .arg(QDateTime::fromMSecsSinceEpoch(minTime).toString(Qt::SystemLocaleShortDate));
        }
        else
        {
            extra = tr(
                "Selected files were recorded between %1 and %2, "
                "but only files that were recorded in the last %n days can be uploaded. "
                "You can change this in camera archive settings.", "", days)
                .arg(QDateTime::fromMSecsSinceEpoch(minTime).toString(Qt::SystemLocaleShortDate))
                .arg(QDateTime::fromMSecsSinceEpoch(maxTime).toString(Qt::SystemLocaleShortDate));
        }

        QnMessageBox::warning(mainWindow(), title, extra);
        return false;
    }

    if (upload->allHaveStatus(WearablePayload::ChunksTakenByFileInQueue))
    {
        QnMessageBox::warning(mainWindow(),
            tr("Selected files cover periods for which videos are already being uploaded", "", count),
            tr("You can upload these files to a different instance of a Wearable Camera.", "", count));
        return false;
    }

    if (upload->allHaveStatus(WearablePayload::ChunksTakenOnServer))
    {
        QnMessageBox::warning(mainWindow(),
            tr("Selected files cover periods for which videos have already been uploaded", "", count),
            tr("You can upload these files to a different instance of a Wearable Camera.", "", count));
        return false;
    }

    if (upload->someHaveStatus(WearablePayload::ServerError))
        return false; //< Ignore it as the user is likely already seeing "reconnecting to server" dialog.

    if (upload->someHaveStatus(WearablePayload::NoSpaceOnServer))
    {
        QnMessageBox::warning(mainWindow(),
            tr("Not enough space on server storage"));
        return false;
    }

    bool performExtendedCheck = true;
    if (upload->someHaveStatus(WearablePayload::StorageCleanupNeeded))
    {
        if (fixStorageCleanupUpload(upload))
            performExtendedCheck = false;
        else
            return false;
    }

    if (performExtendedCheck && !upload->allHaveStatus(WearablePayload::Valid))
    {
        QString extendedMessage;
        int lines = 0;
        for (const WearablePayload& payload : upload->elements)
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

        if (!upload->someHaveStatus(WearablePayload::Valid))
        {
            QnMessageBox::warning(mainWindow(),
                tr("Selected files will not be uploaded"),
                extendedMessage);
            return false;
        }
        else
        {
            QDialogButtonBox::StandardButton button = QnMessageBox::warning(mainWindow(),
                tr("Some files will not be uploaded"),
                extendedMessage,
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            if (button == QDialogButtonBox::Cancel)
                return false;
        }
    }

    return true;
}

bool QnWorkbenchWearableHandler::fixFolderUpload(const QString& path, const QnSecurityCamResourcePtr& camera, WearableUpload* upload)
{
    NX_ASSERT(upload);

    WearableUpload copy;

    for (const WearablePayload& payload: upload->elements)
    {
        switch (payload.status)
        {
            case WearablePayload::FileDoesntExist:
            case WearablePayload::UnsupportedFormat:
            case WearablePayload::NoTimestamp:
            case WearablePayload::ChunksTakenByFileInQueue:
            case WearablePayload::ChunksTakenOnServer:
                // Just ignore these silently.
                continue; 
            default:
                copy.elements.push_back(payload);
        }
    }

    if (copy.elements.empty())
    {
        QnMessageBox::warning(mainWindow(),
            tr("No new files to upload in selected folder"),
            path);
        return false;
    }

    *upload = copy;
    return fixFileUpload(camera, upload);
}

bool QnWorkbenchWearableHandler::fixStorageCleanupUpload(WearableUpload* upload)
{
    int n = upload->elements.size();

    bool fixed = QnMessageBox::warning(mainWindow(),
        tr("Some footage may be deleted after uploading these files", 0, n),
        tr("There is not much free space left on server storage. "
            "Some old footage may be deleted to free up space. "
            "Note that if selected files happen to be the oldest on the server, "
            "they will be deleted right after being uploaded.", 0, n)
        + lit("\n\n")
        + tr("To prevent this you can add additional storage. "
            "You can also control which footage will be deleted first by changing "
            "archive keep time in camera settings.")
        + lit("\n\n")
        + tr("Upload anyway?"),
        QDialogButtonBox::Yes | QDialogButtonBox::Cancel) == QDialogButtonBox::Yes;

    if (fixed)
    {
        for (WearablePayload& payload : upload->elements)
            if (payload.status == WearablePayload::StorageCleanupNeeded)
                payload.status = WearablePayload::Valid;
    }

    return fixed;
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
    QnSecurityCamResourcePtr camera =
        resourcePool()->getResourceById<QnSecurityCamResource>(state.cameraId);
    if (!camera)
        return;

    /* Fire an error only on upload failure. */
    if (state.error == WearableState::UploadFailed)
    {
        QnMessageBox::critical(mainWindow(),
            tr("Could not finish upload to %1").arg(camera->getName()),
            tr("Make sure there is enough space on server storage."));
        return;
    }


    /* Update chunks if we've just finished consume request. */
    if (state.consumeProgress == 100 && state.status == WearableState::Consuming)
    {
        QnCachingCameraDataLoaderPtr loader =
            qnClientModule->cameraDataManager()->loader(camera, /*create=*/true);
        loader->invalidateCachedData();
        loader->load(/*forced=*/true);
    }
}

QString QnWorkbenchWearableHandler::calculateExtendedErrorMessage(const WearablePayload& upload)
{
    static const auto kNDash = QString::fromWCharArray(L"\x2013");
    const QString fileName = QFileInfo(upload.path).fileName() + lit(" ") + kNDash;

    switch (upload.status)
    {
    case WearablePayload::Valid:
        return QString();
    case WearablePayload::UnsupportedFormat:
        return tr("%1 has unsupported format.").arg(fileName);
    case WearablePayload::NoTimestamp:
        return tr("%1 does not have timestamp.").arg(fileName);
    case WearablePayload::FootagePastMaxDays:
        return tr("%1 is older than allowed in camera archive settings.").arg(fileName);
    case WearablePayload::ChunksTakenByFileInQueue:
        return tr("%1 covers period for which video has already been uploaded.").arg(fileName);
    case WearablePayload::ChunksTakenOnServer:
        return tr("%1 covers period for which video is being uploaded.").arg(fileName);
    default:
        NX_ASSERT(false);
        return QString();
    }
}
