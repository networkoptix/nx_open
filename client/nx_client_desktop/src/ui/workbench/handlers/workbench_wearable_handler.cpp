#include "workbench_wearable_handler.h"

#include <QtWidgets/QAction>

#include <common/common_module.h>

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

using namespace nx::client::desktop;
using namespace nx::client::desktop::ui;

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
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnWorkbenchWearableHandler::at_resourcePool_resourceAdded);
    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnWorkbenchWearableHandler::at_context_userChanged);
}

QnWorkbenchWearableHandler::~QnWorkbenchWearableHandler()
{
}

qreal QnWorkbenchWearableHandler::calculateProgress(const UploadState& upload, bool processed)
{
    if (processed)
        return 1.0;

    switch (upload.status)
    {
        case UploadState::CreatingUpload:
            return 0.1;
        case UploadState::Uploading:
            return 0.1 + 0.8 * upload.uploaded / upload.size;
        case UploadState::Checking:
        case UploadState::Done:
            return 0.9;
        default:
            return 0.0;
    }
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

    QPointer<QObject> guard(this);
    auto callback =
        [this, guard, server](bool success, rest::Handle, const QnJsonRestResult& reply)
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

    server->restConnection()->addWearableCamera(name, callback, thread());
}

void QnWorkbenchWearableHandler::at_uploadWearableCameraFileAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    auto camera = parameters.resource().dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;
    QnMediaServerResourcePtr server = camera->getParentServer();

    QStringList filters;
    filters << tr("Video (*.avi *.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp)");
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

    WearablePayloadList uploads = qnClientModule->wearableManager()->checkUploads(camera, paths);
    int count = uploads.size();

    if (WearablePayload::allHaveStatus(uploads, WearablePayload::UnsupportedFormat))
    {
        QnMessageBox::critical(mainWindow(),
            tr("Selected file format(s) are not supported", 0, count));
        return;
    }

    if (WearablePayload::allHaveStatus(uploads, WearablePayload::NoTimestamp))
    {
        QnMessageBox::critical(mainWindow(),
            tr("Selected file(s) do not have timestamp", 0, count),
            tr("Only video files with correct timestamp are supported."));
        return;
    }

    if (!qnClientModule->wearableManager()->addUploads(camera, uploads))
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
        return;
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
