#include "wearable_worker.h"
#include "server_request_storage.h"
#include "upload_manager.h"
#include "wearable_payload.h"

#include <QtCore/QTimer>

#include <api/model/wearable_status_reply.h>
#include <api/server_rest_connection.h>
#include <client/client_module.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <rest/server/json_rest_result.h>

#include <nx/utils/guarded_callback.h>

namespace nx::vms::client::desktop {

namespace {
/**
 * Using TTL of 10 mins for uploads. This shall be enough even for the most extreme cases.
 * Also note that undershooting is not a problem here as a file that's currently open won't be
 * deleted.
 */
const qint64 kDefaultUploadTtl = 1000 * 60 * 10;

const qint64 kLockTtlMSec = 1000 * 15;

const qint64 kUploadPollPeriodMSec = 1000 * 5;
const qint64 kConsumePollPeriodMSec = 1000;

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
    timer->start(kUploadPollPeriodMSec);

    // The best way to do this would have been by integrating timers into the FSM,
    // but that would've created another source of hard-to-find bugs. So we go the easy route.
    QTimer* quickTimer = new QTimer(this);
    connect(quickTimer, &QTimer::timeout, this,
        [this]
        {
            if (d->state.status == WearableState::Consuming)
                pollExtend();
        });
    quickTimer->start(kConsumePollPeriodMSec);
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
    // No point to update if we're already sending extend requests non-stop.
    if (isWorking())
        return;

    // Make sure we don't spam the server.
    if (d->waitingForStatusReply)
        return;
    d->waitingForStatusReply = true;

    const auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            d->requests.releaseHandle(handle);
            QnWearableStatusReply reply = result.deserialized<QnWearableStatusReply>();
            handleStatusFinished(success, reply);
        });

    d->requests.storeHandle(d->connection()->wearableCameraStatus(d->camera, callback, thread()));
}

bool WearableWorker::addUpload(const WearablePayloadList& payloads)
{
    if (d->state.status == WearableState::LockedByOtherClient)
        return false;

    for (const WearablePayload& payload : payloads)
    {
        WearableState::EnqueuedFile file;
        file.path = payload.path;
        file.startTimeMs = payload.local.period.startTimeMs;
        file.uploadPeriod = payload.remote.period;
        d->state.queue.push_back(file);
    }

    if (isWorking())
    {
        emit stateChanged(d->state);
        return true;
    }

    const auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            d->requests.releaseHandle(handle);
            QnWearableStatusReply reply = result.deserialized<QnWearableStatusReply>();
            handleLockFinished(success, reply);
        });
    d->requests.storeHandle(d->connection()->lockWearableCamera(
        d->camera,
        d->user,
        kLockTtlMSec,
        callback,
        thread()));

    return true;
}

void WearableWorker::cancel()
{
    handleStop();

    if(isWorking())
        d->state.status = WearableState::Unlocked;

    emit stateChanged(d->state);
    emit finished();
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

        const auto callback = nx::utils::guarded(this,
            [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
            {
                d->requests.releaseHandle(handle);
                QnWearableStatusReply reply = result.deserialized<QnWearableStatusReply>();
                handleUnlockFinished(success, reply);
            });
        d->requests.storeHandle(d->connection()->releaseWearableCameraLock(
            d->camera,
            d->lockToken,
            callback,
            thread()));
        return;
    }

    WearableState::EnqueuedFile file = d->state.currentFile();

    UploadState config;
    config.source = file.path;
    config.ttl = kDefaultUploadTtl;
    // TODO: Check if it is necessary. Configured to keep the original behavior.
    config.uploadAllChunks = true;
    config.recreateFile = true;

    QString uploadId = qnClientModule->uploadManager()->addUpload(
        d->server, config, this,
        [this](const UploadState& upload)
        {
            handleUploadProgress(upload);
        }
    );

    if (uploadId.isEmpty())
    {
        handleFailure(WearableState::UploadFailed, config.errorMessage);
        return;
    }

    d->state.status = WearableState::Uploading;
    d->state.currentUpload = qnClientModule->uploadManager()->state(uploadId);

    emit stateChanged(d->state);
}

WearableState::Status WearableWorker::calculateNewStatus(WearableState::Error error)
{
    switch (error)
    {
    case WearableState::LockRequestFailed:
    case WearableState::CameraSnatched:
        return WearableState::LockedByOtherClient;
    case WearableState::UploadFailed:
    case WearableState::ConsumeRequestFailed:
        return WearableState::Unlocked;
    default:
        return WearableState::Unlocked;
    }
}

QString WearableWorker::calculateErrorMessage(WearableState::Error /*error*/, const QString& errorMessage)
{
    if (!errorMessage.isEmpty())
        return errorMessage;

    // There really is no reason to explain what went wrong in the FSM.
    return tr("Failed to send request to the server.");
}

void WearableWorker::handleFailure(WearableState::Error error, const QString& errorMessage)
{
    handleStop();

    d->state.queue.clear();
    d->state.status = calculateNewStatus(error);
    d->state.error = error;

    emit stateChanged(d->state);
    emit this->error(d->state, calculateErrorMessage(error, errorMessage));
    emit finished();
}

void WearableWorker::handleStop()
{
    d->requests.cancelAllRequests();

    if (!isWorking())
        return;

    if (d->state.status == WearableState::Uploading)
        qnClientModule->uploadManager()->cancelUpload(d->state.currentUpload.id);

    d->connection()->releaseWearableCameraLock(
        d->camera,
        d->lockToken,
        nullptr);
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
        if (d->state.status != WearableState::LockedByOtherClient
            || d->state.lockUserId != result.userId)
        {
            d->state.status = WearableState::LockedByOtherClient;
            d->state.lockUserId = result.userId;

            emit stateChanged(d->state);
        }
    }
    else
    {
        if (d->state.status == WearableState::LockedByOtherClient)
        {
            d->state.status = WearableState::Unlocked;
            d->state.lockUserId = QnUuid();

            emit stateChanged(d->state);
        }
    }
}

void WearableWorker::handleLockFinished(bool success, const QnWearableStatusReply& result)
{
    if (!success)
    {
        handleFailure(WearableState::LockRequestFailed);
        return;
    }

    // Already working => that's a stray reply, ignore it.
    if (isWorking())
        return;

    if (!result.success)
    {
        d->state.lockUserId = result.userId;
        handleFailure(WearableState::CameraSnatched);
        return;
    }

    d->state.status = WearableState::Locked;
    d->lockToken = result.token;

    emit stateChanged(d->state);
    processCurrentFile();
}

void WearableWorker::handleExtendFinished(bool success, const QnWearableStatusReply& result)
{
    // Errors are safe to ignore here as we'll resend the command soon anyway.
    if (!success)
        return;

    // Already done & sorted => that's a stray reply, ignore it.
    if (!isWorking())
        return;

    if (!result.success)
    {
        handleFailure(WearableState::CameraSnatched);
        return;
    }

    if(d->state.status == WearableState::Consuming)
    {
        d->state.consumeProgress = result.consuming ? std::min(99, result.progress) : 100;
        emit stateChanged(d->state);

        if (!result.consuming)
            processNextFile();
    }
}

void WearableWorker::handleUnlockFinished(bool success, const QnWearableStatusReply& result)
{
    // Not working => that's a stray reply, ignore it.
    if (!isWorking())
        return;

    // User has started another upload while we were waiting for a reply, ignore this one.
    if (!d->state.isDone())
        return;

    d->state.status = WearableState::Unlocked;

    emit stateChanged(d->state);

    d->state.queue.clear();
    d->state.currentIndex = 0;

    emit finished();
}

void WearableWorker::handleUploadProgress(const UploadState& state)
{
    // In theory we should never get here in non-working state, but we want to feel safe.
    if (!isWorking())
        return;

    d->state.currentUpload = state;

    if (state.status == UploadState::Error)
    {
        handleFailure(WearableState::UploadFailed, state.errorMessage);
        return;
    }

    if (state.status == UploadState::Done)
    {
        d->state.status = WearableState::Consuming;
        d->state.consumeProgress = 0;

        const auto callback = nx::utils::guarded(this,
            [this](bool success, rest::Handle handle, const rest::ServerConnection::EmptyResponseType&)
            {
                d->requests.releaseHandle(handle);
                handleConsumeStarted(success);
            });

        d->requests.storeHandle(d->connection()->consumeWearableCameraFile(
            d->camera,
            d->lockToken,
            d->state.currentUpload.destination,
            d->state.currentFile().startTimeMs,
            callback,
            thread()));
    }

    emit stateChanged(d->state);
}

void WearableWorker::handleConsumeStarted(bool success)
{
    if (!success)
        handleFailure(WearableState::ConsumeRequestFailed);

    // Do nothing here, polling is done in timer event.
}

void WearableWorker::pollExtend()
{
    // Not working => no reason to poll.
    if (!isWorking())
        return;

    const auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            d->requests.releaseHandle(handle);
            QnWearableStatusReply reply = result.deserialized<QnWearableStatusReply>();
            handleExtendFinished(success, reply);
        });
    d->requests.storeHandle(d->connection()->extendWearableCameraLock(
        d->camera,
        d->user,
        d->lockToken,
        kLockTtlMSec,
        callback,
        thread()));
}

} // namespace nx::vms::client::desktop


