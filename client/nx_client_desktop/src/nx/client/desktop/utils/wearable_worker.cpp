#include "wearable_worker.h"

#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

#include <rest/server/json_rest_result.h>
#include <api/model/wearable_status_reply.h>
#include <api/server_rest_connection.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <plugins/resource/avi/avi_resource.h>
#include <plugins/resource/avi/avi_archive_delegate.h>

#include <client/client_module.h>

#include "server_request_storage.h"
#include "upload_manager.h"

namespace nx {
namespace client {
namespace desktop {

namespace {
/**
 * Using TTL of 10 mins for uploads. This shall be enough even for the most extreme cases.
 * Also note that undershooting is not a problem here as a file that's currently open won't be
 * deleted.
 */
const qint64 kDefaultUploadTtl = 1000 * 60 * 10;

const qint64 kLockTtlMSec = 1000 * 15;

const qint64 kPollPeriodMSec = 1000 * 5;

} // namespace

struct WearableWorker::Private
{
    Private(const QnSecurityCamResourcePtr& camera) :
        camera(camera),
        server(camera->getParentServer()),
        requests(camera->getParentServer())
    {
        state.cameraId = camera->getId();
    }

    rest::QnConnectionPtr connection() const
    {
        return server->restConnection();
    }

    QnSecurityCamResourcePtr camera;
    QnMediaServerResourcePtr server;
    QnUserResourcePtr user;

    WearableState state;
    QnUuid lockToken;

    ServerRequestStorage requests;
    bool waitingForStatusReply = false;
};

WearableWorker::WearableWorker(
    const QnSecurityCamResourcePtr& camera,
    const QnUserResourcePtr& user,
    QObject* parent)
    :
    base_type(parent),
    QnCommonModuleAware(parent),
    d(new Private(camera))
{
    d->user = user;

    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &WearableWorker::pollExtend);
    timer->start(kPollPeriodMSec);
}

WearableWorker::~WearableWorker()
{
    d->requests.cancelAllRequests();
}

WearableState WearableWorker::state() const
{
    return d->state;
}

bool WearableWorker::isWorking() const
{
    return d->state.isRunning();
}

void WearableWorker::updateState()
{
    if (isWorking())
        return; //< No point to update.

    if (d->waitingForStatusReply)
        return; //< Too many updates.
    d->waitingForStatusReply = true;

    auto callback =
        [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            d->requests.releaseHandle(handle);
            QnWearableStatusReply reply = result.deserialized<QnWearableStatusReply>();
            handleStatusFinished(success, reply);
        };

    d->requests.storeHandle(d->connection()->wearableCameraStatus(d->camera, callback, thread()));
}

bool WearableWorker::addUpload(const QString& path, WearableError* error)
{
    if (d->state.status == WearableState::LockedByOtherClient)
    {
        error->message = calculateLockedMessage();
        return false;
    }

    QnAviResourcePtr resource(new QnAviResource(path));
    QnAviArchiveDelegatePtr delegate(resource->createArchiveDelegate());
    bool opened = delegate->open(resource) && delegate->findStreams();

    if (!opened || !delegate->hasVideo() || resource->hasFlags(Qn::still_image))
    {
        error->message = tr("Selected file format is not supported");
        error->extraMessage = tr("Use .MKV, .AVI, or .MP4 video files.");
        return false;
    }

    qint64 startTimeMs = delegate->startTime() / 1000;

    if (startTimeMs == 0)
    {
        QString startTimeString = QLatin1String(delegate->getTagValue("creation_time"));
        QDateTime startDateTime = QDateTime::fromString(startTimeString, Qt::ISODate);
        if (startDateTime.isValid())
            startTimeMs = startDateTime.toMSecsSinceEpoch();
    }

    if (startTimeMs == 0)
    {
        error->message = tr("Selected file does not have timestamp");
        error->extraMessage = tr("Only video files with correct timestamp are supported.");
        return false;
    }

    WearableState::EnqueuedFile file;
    file.path = path;
    file.startTimeMs = startTimeMs;
    d->state.queue.push_back(file);

    if (d->state.status != WearableState::Unlocked)
    {
        emit stateChanged(d->state);
        return true;
    }

    auto callback =
        [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            d->requests.releaseHandle(handle);
            QnWearableStatusReply reply = result.deserialized<QnWearableStatusReply>();
            handleLockFinished(success, reply);
        };
    d->requests.storeHandle(d->connection()->lockWearableCamera(
        d->camera,
        d->user,
        kLockTtlMSec,
        callback,
        thread()));

    return true;
}

void WearableWorker::processNextFile()
{
    d->state.currentIndex++;
    processCurrentFile();
}

void WearableWorker::processCurrentFile()
{
    if (d->state.isDone())
    {
        d->state.status = WearableState::Locked;

        auto callback =
            [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
            {
                d->requests.releaseHandle(handle);
                QnWearableStatusReply reply = result.deserialized<QnWearableStatusReply>();
                handleUnlockFinished(success, reply);
            };
        d->requests.storeHandle(d->connection()->releaseWearableCameraLock(
            d->camera,
            d->lockToken,
            callback,
            thread()));
        return;
    }

    WearableState::EnqueuedFile file = d->state.currentFile();

    QString errorMessage;
    QString uploadId = qnClientModule->uploadManager()->addUpload(
        d->server,
        file.path,
        kDefaultUploadTtl,
        &errorMessage,
        this,
        [this](const UploadState& upload) { handleUploadProgress(upload); }
    );
    if (uploadId.isEmpty())
    {
        emit error(d->state, errorMessage);
        processNextFile();
    }

    d->state.status = WearableState::Uploading;
    d->state.currentUpload = qnClientModule->uploadManager()->state(uploadId);

    emit stateChanged(d->state);
}

void WearableWorker::handleStatusFinished(bool success, const QnWearableStatusReply& result)
{
    d->waitingForStatusReply = false;

    // Errors are perfectly safe to ignore here.
    if (!success || !result.success)
        return;

    // Already in working state => we're not getting any new info from this message.
    if (isWorking())
        return;

    if (result.locked)
    {
        d->state.status = WearableState::LockedByOtherClient;
        d->state.lockUserId = result.userId;

        emit stateChanged(d->state);
    }
    else
    {
        d->state.status = WearableState::Unlocked;
        d->state.lockUserId = QnUuid();

        emit stateChanged(d->state);
    }
}

void WearableWorker::handleLockFinished(bool success, const QnWearableStatusReply& result)
{
    if (!success)
        return;

    if (!result.success)
    {
        d->state.status = WearableState::LockedByOtherClient;
        d->state.lockUserId = result.userId;
        d->state.queue.clear();

        emit error(d->state, calculateLockedMessage());
        emit stateChanged(d->state);
    }
    else
    {
        d->state.status = WearableState::Locked;
        d->lockToken = result.token;

        emit stateChanged(d->state);
        processCurrentFile();
    }
}

void WearableWorker::handleExtendFinished(bool success, const QnWearableStatusReply& result)
{
    if (!success)
        return;

    if (!isWorking())
        return;

    if (!result.success)
    {
        // Extend finished with an error => someone has snatched our camera! Not likely to happen.
        d->state.status = WearableState::LockedByOtherClient;
        d->state.lockUserId = result.userId;

        emit stateChanged(d->state);
        return;
    }

    if(d->state.status == WearableState::Consuming)
    {
        d->state.consumeProgress = result.progress;
        emit stateChanged(d->state);

        if (!result.consuming)
            processNextFile();
    }
}

void WearableWorker::handleUnlockFinished(bool success, const QnWearableStatusReply& result)
{
    d->state.status = WearableState::Unlocked;

    emit stateChanged(d->state);

    d->state.queue.clear();
    d->state.currentIndex = 0;
}

void WearableWorker::handleUploadProgress(const UploadState& state)
{
    d->state.currentUpload = state;

    if (state.status == UploadState::Error)
    {
        emit error(d->state, state.errorMessage);
    }

    if (state.status == UploadState::Done)
    {
        d->state.status = WearableState::Consuming;
        d->state.consumeProgress = 0;

        auto callback =
            [this](bool success, rest::Handle handle, const rest::ServerConnection::EmptyResponseType&)
            {
                d->requests.releaseHandle(handle);
                handleConsumeStarted(success);
            };

        d->requests.storeHandle(d->connection()->consumeWearableCameraFile(
            d->camera,
            d->lockToken,
            d->state.currentUpload.id,
            d->state.currentFile().startTimeMs,
            callback,
            thread()));
    }

    emit stateChanged(d->state);
}

void WearableWorker::handleConsumeStarted(bool success)
{
    if (!success)
        return;

    // Do nothing here, polling is done in timer event.
}

void WearableWorker::pollExtend()
{
    WearableState::Status status = d->state.status;
    if (!isWorking())
        return;

    auto callback =
        [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            d->requests.releaseHandle(handle);
            QnWearableStatusReply reply = result.deserialized<QnWearableStatusReply>();
            handleExtendFinished(success, reply);
        };
    d->requests.storeHandle(d->connection()->extendWearableCameraLock(
        d->camera,
        d->user,
        d->lockToken,
        kLockTtlMSec,
        callback,
        thread()));
}

QString WearableWorker::calculateUserName(const QnUuid& userId)
{
    QnResourcePtr resource = resourcePool()->getResourceById(userId);
    if (!resource)
        return QString();
    return resource->getName();
}

QString WearableWorker::calculateLockedMessage()
{
    QString userName = calculateUserName(d->state.lockUserId);
    if (userName.isEmpty())
        return tr("Could not start upload as another user is currently uploading footage to this camera.");
    else
        return tr("Could not start upload as user \"%1\" is currently uploading footage to this camera.").arg(userName);

}

} // namespace desktop
} // namespace client
} // namespace nx


