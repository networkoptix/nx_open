// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "virtual_camera_action_handler.h"

#include <QtCore/QDirIterator>
#include <QtGui/QAction>

#include <api/model/virtual_camera_reply.h>
#include <api/server_rest_connection.h>
#include <camera/camera_data_manager.h>
#include <client/client_module.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/string.h>
#include <nx/utils/unicode_chars.h>
#include <nx/vms/client/core/resource/data_loaders/caching_camera_data_loader.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_tab.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/text/human_readable.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/new_virtual_camera_dialog.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_state_manager.h>

#include "virtual_camera_manager.h"
#include "virtual_camera_payload.h"
#include "virtual_camera_state.h"

namespace nx::vms::client::desktop {

using namespace nx::utils;

namespace {

const int kMaxLinesInExtendedErrorMessage = 10;

} // namespace

class QnVirtualCameraSessionDelegate:
    public QObject,
    public QnSessionAwareDelegate
{
public:
    QnVirtualCameraSessionDelegate(QObject* parent) :
        QObject(parent),
        QnSessionAwareDelegate(parent)
    {
    }

    virtual bool tryClose(bool force) override
    {
        auto systemContext = system();
        if (!force)
        {
            QnResourceList resources;
            for (const VirtualCameraState& state:
                systemContext->virtualCameraManager()->runningUploads())
            {
                QnResourcePtr resource = systemContext->resourcePool()->getResourceById(state.cameraId);
                if (resource)
                    resources.push_back(resource);
            }

            if (!ui::messages::Resources::stopVirtualCameraUploads(mainWindowWidget(), resources))
                return false;
        }

        systemContext->virtualCameraManager()->cancelAllUploads();
        return true;
    }
};

VirtualCameraActionHandler::VirtualCameraActionHandler(
    WindowContext* windowContext,
    QObject* parent)
    :
    base_type(parent),
    WindowContextAware(windowContext)
{
    using namespace menu;

    new QnVirtualCameraSessionDelegate(this);

    connect(action(AddVirtualCameraAction), &QAction::triggered, this,
        &VirtualCameraActionHandler::at_newVirtualCameraAction_triggered);
    connect(action(MainMenuAddVirtualCameraAction), &QAction::triggered, this,
        &VirtualCameraActionHandler::at_newVirtualCameraAction_triggered);
    connect(action(UploadVirtualCameraFileAction), &QAction::triggered, this,
        &VirtualCameraActionHandler::at_uploadVirtualCameraFileAction_triggered);
    connect(action(UploadVirtualCameraFolderAction), &QAction::triggered, this,
        &VirtualCameraActionHandler::at_uploadVirtualCameraFolderAction_triggered);
    connect(action(CancelVirtualCameraUploadsAction), &QAction::triggered, this,
        &VirtualCameraActionHandler::at_cancelVirtualCameraUploadsAction_triggered);
    connect(system()->resourcePool(), &QnResourcePool::resourceAdded, this,
        &VirtualCameraActionHandler::at_resourcePool_resourceAdded);

    // FIXME: #sivanov Connect to all contexts.
    auto systemContext = appContext()->currentSystemContext();
    connect(systemContext->virtualCameraManager(), &VirtualCameraManager::stateChanged, this,
        &VirtualCameraActionHandler::at_virtualCameraManager_stateChanged);

    const auto& manager = windowContext->localNotificationsManager();
    connect(manager, &nx::vms::client::desktop::workbench::LocalNotificationsManager::cancelRequested, this,
        [this](const QnUuid& progressId)
        {
            if (const auto camera = cameraByProgressId(progressId))
                menu()->trigger(menu::CancelVirtualCameraUploadsAction, camera);
        });

    connect(manager, &nx::vms::client::desktop::workbench::LocalNotificationsManager::interactionRequested, this,
        [this](const QnUuid& progressId)
        {
            if (const auto camera = cameraByProgressId(progressId))
                menu()->trigger(menu::CameraSettingsAction, camera);
        });
}

VirtualCameraActionHandler::~VirtualCameraActionHandler()
{
}

void VirtualCameraActionHandler::maybeOpenCurrentSettings()
{
    if (m_currentCameraUuid.isNull())
        return;

    auto camera = system()->resourcePool()->getResourceById<QnSecurityCamResource>(
        m_currentCameraUuid);
    if (!camera)
        return;

    m_currentCameraUuid = QnUuid();
    menu()->trigger(menu::CameraSettingsAction,
        menu::Parameters(camera, {{Qn::FocusTabRole, (int)CameraSettingsTab::general}}));
}

void VirtualCameraActionHandler::at_newVirtualCameraAction_triggered()
{
    if (!system()->connection())
        return;

    const auto params = menu()->currentParameters(sender());
    const auto selectedServer = params.resource().dynamicCast<QnMediaServerResource>();

    std::unique_ptr<QnNewVirtualCameraDialog> dialog
        = std::make_unique<QnNewVirtualCameraDialog>(mainWindowWidget(), selectedServer);

    if (dialog->exec() != QDialog::Accepted)
        return;

    QString name = dialog->name();
    QnMediaServerResourcePtr server = dialog->server();

    const auto callback = nx::utils::guarded(this,
        [this, server](bool success, rest::Handle, const nx::network::rest::JsonResult& reply)
        {
            if (!success)
            {
                QnMessageBox::critical(
                    mainWindowWidget(),
                    tr("Failed to add virtual camera")
                );
                return;
            }

            m_currentCameraUuid = reply.deserialized<QnVirtualCameraReply>().id;
            maybeOpenCurrentSettings();
        });

    system()->connectedServerApi()->addVirtualCamera(server->getId(), name, callback, thread());
}

void VirtualCameraActionHandler::at_uploadVirtualCameraFileAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnSecurityCamResourcePtr camera = parameters.resource().dynamicCast<QnSecurityCamResource>();
    if (!camera || !camera->getParentServer())
        return;

    QStringList paths = QFileDialog::getOpenFileNames(
        mainWindowWidget(),
        tr("Open Virtual Camera Recordings..."),
        QString(),
        QnCustomFileDialog::createFilter({
            QnCustomFileDialog::kVideoFilter,
            QnCustomFileDialog::kAllFilesFilter
            }),
        /*selectedFilter*/ nullptr,
        QnCustomFileDialog::fileDialogOptions()
    );
    if (paths.isEmpty())
        return;

    // TODO: #virtualCamera requested by rvasilenko as copypaste from totalcmd doesn't work without
    // this line. Maybe move directly to QFileDialog?
    for(QString& path: paths)
        path = path.trimmed();

    auto systemContext = SystemContext::fromResource(camera);
    systemContext->virtualCameraManager()->prepareUploads(camera, paths, this,
        [this, camera](VirtualCameraUpload upload)
        {
            if(fixFileUpload(camera, &upload))
                uploadValidFiles(camera, upload.elements);
        });
}

void VirtualCameraActionHandler::at_uploadVirtualCameraFolderAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnSecurityCamResourcePtr camera = parameters.resource().dynamicCast<QnSecurityCamResource>();
    if (!camera || !camera->getParentServer())
        return;

    QString path = QFileDialog::getExistingDirectory(mainWindowWidget(),
        tr("Open Virtual Camera Recordings..."),
        QString(),
        QnCustomFileDialog::directoryDialogOptions()
    );
    if (path.isEmpty())
        return;

    static const QStringList kVideoFormats = QnCustomFileDialog::kVideoFilter.second;
    QStringList videoExtensions;
    std::transform(kVideoFormats.cbegin(), kVideoFormats.cend(),
        std::back_inserter(videoExtensions),
        [](const QString& extension) { return "*." + extension; });

    QStringList files;
    QDirIterator iterator(path, videoExtensions, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext())
        files.push_back(iterator.next());

    if (files.empty())
    {
        QnMessageBox::warning(mainWindowWidget(),
            tr("No video files found in selected folder"),
            path);
        return;
    }

    auto systemContext = SystemContext::fromResource(camera);
    systemContext->virtualCameraManager()->prepareUploads(camera, files, this,
        [this, path, camera](VirtualCameraUpload upload)
        {
            if (fixFolderUpload(path, camera, &upload))
                uploadValidFiles(camera, upload.elements);
        });
}

void VirtualCameraActionHandler::at_cancelVirtualCameraUploadsAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnSecurityCamResourcePtr camera = parameters.resource().dynamicCast<QnSecurityCamResource>();
    if (!camera || !camera->getParentServer())
        return;

    auto systemContext = SystemContext::fromResource(camera);
    VirtualCameraState state = systemContext->virtualCameraManager()->state(camera);
    if (!state.isCancellable())
        return;

    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Stop uploading?"), tr("Already uploaded files will be kept."),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, mainWindowWidget());

    dialog.addCustomButton(QnMessageBoxCustomButton::Stop,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    if (dialog.exec() != QDialogButtonBox::Cancel)
        systemContext->virtualCameraManager()->cancelUploads(camera);
}

bool VirtualCameraActionHandler::fixFileUpload(
    const QnSecurityCamResourcePtr& camera,
    VirtualCameraUpload* upload)
{
    int count = upload->elements.size();

    if (upload->allHaveStatus(VirtualCameraPayload::UnsupportedFormat))
    {
        QnMessageBox::warning(mainWindowWidget(),
            tr("Selected file formats are not supported", "", count),
            tr("Use .MKV, .AVI, .MP4 or other video files."));
        return false;
    }

    if (upload->allHaveStatus(VirtualCameraPayload::NoTimestamp))
    {
        QnMessageBox::warning(mainWindowWidget(),
            tr("Selected files do not have timestamps", "", count),
            tr("Only video files with correct timestamp are supported."));
        return false;
    }

    if (upload->allHaveStatus(VirtualCameraPayload::FootagePastMaxDays))
    {
        qint64 minTime = std::numeric_limits<qint64>::max();
        qint64 maxTime = std::numeric_limits<qint64>::min();
        for (const VirtualCameraPayload& payload: upload->elements)
        {
            minTime = std::min(minTime, payload.local.period.startTimeMs);
            maxTime = std::max(maxTime, payload.local.period.endTimeMs());
        }

        QString title = tr("Selected files are too old", "", count);

        QLocale locale = QLocale::system();

        const int days = duration_cast<std::chrono::days>(camera->maxPeriod()).count();
        QString extra;
        if (count == 1)
        {
            extra = tr(
                "Selected file was recorded on %1, "
                "but only files that were recorded in the last %n days can be uploaded. "
                "You can change this in camera archive settings.", "", days)
                .arg(locale.toString(QDateTime::fromMSecsSinceEpoch(minTime), QLocale::ShortFormat));
        }
        else
        {
            extra = tr(
                "Selected files were recorded between %1 and %2, "
                "but only files that were recorded in the last %n days can be uploaded. "
                "You can change this in camera archive settings.", "", days)
                .arg(locale.toString(QDateTime::fromMSecsSinceEpoch(minTime), QLocale::ShortFormat))
                .arg(locale.toString(QDateTime::fromMSecsSinceEpoch(maxTime), QLocale::ShortFormat));
        }

        QnMessageBox::warning(mainWindowWidget(), title, extra);
        return false;
    }

    if (upload->allHaveStatus(VirtualCameraPayload::ChunksTakenByFileInQueue))
    {
        QnMessageBox::warning(mainWindowWidget(),
            tr("Selected files cover periods for which videos are already being uploaded", "", count),
            tr("You can upload these files to a different instance of a Virtual Camera.", "", count));
        return false;
    }

    if (upload->allHaveStatus(VirtualCameraPayload::ChunksTakenOnServer))
    {
        QnMessageBox::warning(mainWindowWidget(),
            tr("Selected files cover periods for which videos have already been uploaded", "", count),
            tr("You can upload these files to a different instance of a Virtual Camera.", "", count));
        return false;
    }

    if (upload->someHaveStatus(VirtualCameraPayload::ServerError))
        return false; //< Ignore it as the user is likely already seeing "reconnecting to server" dialog.

    if (upload->someHaveStatus(VirtualCameraPayload::NoSpaceOnServer))
    {
        QnMessageBox::warning(mainWindowWidget(),
            tr("Not enough space on server storage"));
        return false;
    }

    bool performExtendedCheck = true;
    if (upload->someHaveStatus(VirtualCameraPayload::StorageCleanupNeeded))
    {
        if (fixStorageCleanupUpload(upload))
            performExtendedCheck = false;
        else
            return false;
    }

    if (performExtendedCheck && !upload->allHaveStatus(VirtualCameraPayload::Valid))
    {
        QString extendedMessage;
        int lines = 0;
        for (const VirtualCameraPayload& payload: upload->elements)
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

        if (!upload->someHaveStatus(VirtualCameraPayload::Valid))
        {
            QnMessageBox::warning(mainWindowWidget(),
                tr("Selected files will not be uploaded"),
                extendedMessage);
            return false;
        }
        else
        {
            QDialogButtonBox::StandardButton button = QnMessageBox::warning(mainWindowWidget(),
                tr("Some files will not be uploaded"),
                extendedMessage,
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            if (button == QDialogButtonBox::Cancel)
                return false;
        }
    }

    return true;
}

bool VirtualCameraActionHandler::fixFolderUpload(const QString& path, const QnSecurityCamResourcePtr& camera, VirtualCameraUpload* upload)
{
    NX_ASSERT(upload);

    VirtualCameraUpload copy;

    for (const VirtualCameraPayload& payload: upload->elements)
    {
        switch (payload.status)
        {
            case VirtualCameraPayload::FileDoesntExist:
            case VirtualCameraPayload::UnsupportedFormat:
            case VirtualCameraPayload::NoTimestamp:
            case VirtualCameraPayload::ChunksTakenByFileInQueue:
            case VirtualCameraPayload::ChunksTakenOnServer:
                // Just ignore these silently.
                continue;
            default:
                copy.elements.push_back(payload);
        }
    }

    if (copy.elements.empty())
    {
        QnMessageBox::warning(mainWindowWidget(),
            tr("No new files to upload in selected folder"),
            path);
        return false;
    }

    *upload = copy;
    return fixFileUpload(camera, upload);
}

bool VirtualCameraActionHandler::fixStorageCleanupUpload(VirtualCameraUpload* upload)
{
    int n = upload->elements.size();

    bool fixed = QnMessageBox::warning(mainWindowWidget(),
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
        for (VirtualCameraPayload& payload: upload->elements)
            if (payload.status == VirtualCameraPayload::StorageCleanupNeeded)
                payload.status = VirtualCameraPayload::Valid;
    }

    return fixed;
}

void VirtualCameraActionHandler::uploadValidFiles(
    const QnSecurityCamResourcePtr& camera,
    const VirtualCameraPayloadList& payloads)
{
    VirtualCameraPayloadList validPayloads;
    for (const VirtualCameraPayload& payload: payloads)
        if (payload.status == VirtualCameraPayload::Valid)
            validPayloads.push_back(payload);

    auto systemContext = SystemContext::fromResource(camera);
    if (!systemContext->virtualCameraManager()->addUpload(camera, validPayloads))
    {
        VirtualCameraState state = systemContext->virtualCameraManager()->state(camera);
        NX_ASSERT(state.status == VirtualCameraState::LockedByOtherClient);

        QString message;
        QnResourcePtr resource = system()->resourcePool()->getResourceById(state.lockUserId);
        if (resource)
            message = tr("Could not start upload as user \"%1\" is currently uploading footage to this camera.").arg(resource->getName());
        else
            message = tr("Could not start upload as another user is currently uploading footage to this camera.");

        QnMessageBox::critical(mainWindowWidget(), message);
    }
}

void VirtualCameraActionHandler::at_resourcePool_resourceAdded(const QnResourcePtr &resource)
{
    if (resource->getId() == m_currentCameraUuid)
        maybeOpenCurrentSettings();
}

void VirtualCameraActionHandler::at_virtualCameraManager_stateChanged(const VirtualCameraState& state)
{
    QnSecurityCamResourcePtr camera =
        system()->resourcePool()->getResourceById<QnSecurityCamResource>(state.cameraId);

    if (!camera)
    {
        removeProgress(state.cameraId);
        return;
    }

    // Fire an error only on upload failure.
    if (state.error == VirtualCameraState::UploadFailed)
    {
        QnMessageBox::critical(mainWindowWidget(),
            tr("Could not finish upload to %1").arg(camera->getName()),
            tr("Make sure there is enough space on server storage."));
        removeProgress(state.cameraId);
        return;
    }

    // Update chunks if we've just finished consume request.
    if (state.consumeProgress == 100 && state.status == VirtualCameraState::Consuming)
    {
        auto systemContext = SystemContext::fromResource(camera);
        if (!NX_ASSERT(systemContext))
            return;

        core::CachingCameraDataLoaderPtr loader =
            systemContext->cameraDataManager()->loader(camera, /*create=*/true);
        loader->invalidateCachedData();
        loader->load(/*forced=*/true);
    }

    // Update progress.
    switch (state.status)
    {
        case VirtualCameraState::Locked:
        case VirtualCameraState::Uploading:
        case VirtualCameraState::Consuming:
        {
            const auto& manager = windowContext()->localNotificationsManager();
            const auto progressId = ensureProgress(state.cameraId);
            manager->setDescription(progressId, camera->getName());
            manager->setCancellable(progressId, state.isCancellable());
            manager->setProgress(progressId, state.progress() / 100.0);
            break;
        }

        default:
        {
            removeProgress(state.cameraId);
            break;
        }
    }
}

QnUuid VirtualCameraActionHandler::ensureProgress(const QnUuid& cameraId)
{
    const auto iter = m_currentProgresses.find(cameraId);
    if (iter != m_currentProgresses.end())
        return iter.value();

    const auto progressId = windowContext()->localNotificationsManager()->addProgress(
        tr("Uploading footage"));

    m_currentProgresses.insert(cameraId, progressId);
    return progressId;
}

void VirtualCameraActionHandler::removeProgress(const QnUuid& cameraId)
{
    const auto iter = m_currentProgresses.find(cameraId);
    if (iter == m_currentProgresses.end())
        return;

    windowContext()->localNotificationsManager()->remove(iter.value());
    m_currentProgresses.erase(iter);
}

QnSecurityCamResourcePtr VirtualCameraActionHandler::cameraByProgressId(
    const QnUuid& progressId) const
{
    const auto cameraId = m_currentProgresses.key(progressId); //< No need of extra performance here.
    return cameraId.isNull()
        ? QnSecurityCamResourcePtr()
        : system()->resourcePool()->getResourceById<QnSecurityCamResource>(cameraId);
}

QString VirtualCameraActionHandler::calculateExtendedErrorMessage(const VirtualCameraPayload& upload)
{
    const QString fileName =
        QFileInfo(upload.path).fileName() + lit(" ") + nx::UnicodeChars::kEnDash;

    switch (upload.status)
    {
    case VirtualCameraPayload::Valid:
        return QString();
    case VirtualCameraPayload::UnsupportedFormat:
        return tr("%1 has unsupported format.", "Filename will be substituted").arg(fileName);
    case VirtualCameraPayload::NoTimestamp:
        return tr("%1 does not have timestamp.", "Filename will be substituted").arg(fileName);
    case VirtualCameraPayload::FootagePastMaxDays:
        return tr("%1 is older than allowed in camera archive settings.",
            "Filename will be substituted").arg(fileName);
    case VirtualCameraPayload::ChunksTakenByFileInQueue:
        return tr("%1 covers period for which video has already been uploaded.",
            "Filename will be substituted").arg(fileName);
    case VirtualCameraPayload::ChunksTakenOnServer:
        return tr("%1 covers period for which video is being uploaded.",
            "Filename will be substituted").arg(fileName);
    default:
        NX_ASSERT(false);
        return QString();
    }
}

} // namespace nx::vms::client::desktop
