// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "virtual_camera_worker.h"

#include <QtCore/QTimer>

#include <api/model/virtual_camera_status_reply.h>
#include <api/server_rest_connection.h>
#include <client/client_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/application_context.h>

#include "server_request_storage.h"
#include "upload_manager.h"
#include "virtual_camera_payload.h"

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

struct VirtualCameraWorker::Private
{
    Private(const QnSecurityCamResourcePtr& camera):
        camera(camera)
    {
        state.cameraId = camera->getId();
    }

    QnSecurityCamResourcePtr camera;
    QnUserResourcePtr user;

    VirtualCameraState state;
    QnUuid lockToken;

    ServerRequestStorage requests;
    bool waitingForStatusReply = false;
};

VirtualCameraWorker::VirtualCameraWorker(
    const QnSecurityCamResourcePtr& camera,
    const QnUserResourcePtr& user,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(camera))
{
    d->user = user;

    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &VirtualCameraWorker::pollExtend);
    timer->start(kUploadPollPeriodMSec);

    // The best way to do this would have been by integrating timers into the FSM,
    // but that would've created another source of hard-to-find bugs. So we go the easy route.
    QTimer* quickTimer = new QTimer(this);
    connect(quickTimer, &QTimer::timeout, this,
        [this]
        {
            if (d->state.status == VirtualCameraState::Consuming)
                pollExtend();
        });
    quickTimer->start(kConsumePollPeriodMSec);
}

VirtualCameraWorker::~VirtualCameraWorker()
{
    d->requests.cancelAllRequests();
}

VirtualCameraState VirtualCameraWorker::state() const
{
    return d->state;
}

bool VirtualCameraWorker::isWorking() const
{
    return d->state.isRunning();
}

void VirtualCameraWorker::updateState()
{
    // No point to update if we're already sending extend requests non-stop.
    if (isWorking() || !NX_ASSERT(connection()))
        return;

    // Make sure we don't spam the server.
    if (d->waitingForStatusReply)
        return;
    d->waitingForStatusReply = true;

    const auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
        {
            d->requests.releaseHandle(handle);
            QnVirtualCameraStatusReply reply = result.deserialized<QnVirtualCameraStatusReply>();
            handleStatusFinished(success, reply);
        });

    d->requests.storeHandle(connectedServerApi()->virtualCameraStatus(
        d->camera,
        callback,
        thread()));
}

bool VirtualCameraWorker::addUpload(const VirtualCameraPayloadList& payloads)
{
    if (d->state.status == VirtualCameraState::LockedByOtherClient)
        return false;

    for (const VirtualCameraPayload& payload: payloads)
    {
        VirtualCameraState::EnqueuedFile file;
        file.path = payload.path;
        file.startTimeMs = payload.local.period.startTimeMs;
        file.uploadPeriod = payload.remote.period;
        d->state.queue.push_back(file);
    }

    if (isWorking() || !NX_ASSERT(connection()))
    {
        emit stateChanged(d->state);
        return true;
    }

    const auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
        {
            d->requests.releaseHandle(handle);
            QnVirtualCameraStatusReply reply = result.deserialized<QnVirtualCameraStatusReply>();
            handleLockFinished(success, reply);
        });
    d->requests.storeHandle(connectedServerApi()->lockVirtualCamera(
        d->camera,
        d->user,
        kLockTtlMSec,
        callback,
        thread()));

    return true;
}

void VirtualCameraWorker::cancel()
{
    handleStop();

    if(isWorking())
        d->state.status = VirtualCameraState::Unlocked;

    emit stateChanged(d->state);
    emit finished();
}

void VirtualCameraWorker::processNextFile()
{
    d->state.currentIndex++;
    processCurrentFile();
}

void VirtualCameraWorker::processCurrentFile()
{
    if (d->state.isDone() && NX_ASSERT(connection()))
    {
        d->state.status = VirtualCameraState::Locked;

        const auto callback = nx::utils::guarded(this,
            [this](bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
            {
                d->requests.releaseHandle(handle);
                QnVirtualCameraStatusReply reply = result.deserialized<QnVirtualCameraStatusReply>();
                handleUnlockFinished(success, reply);
            });
        d->requests.storeHandle(connectedServerApi()->releaseVirtualCameraLock(
            d->camera,
            d->lockToken,
            callback,
            thread()));
        return;
    }

    VirtualCameraState::EnqueuedFile file = d->state.currentFile();

    UploadState config;
    config.source = file.path;
    config.ttl = kDefaultUploadTtl;
    // TODO: Check if it is necessary. Configured to keep the original behavior.
    config.uploadAllChunks = true;
    config.recreateFile = true;

    QString uploadId = appContext()->uploadManager()->addUpload(
        d->camera->getParentServer(), config, this,
        [this](const UploadState& upload)
        {
            handleUploadProgress(upload);
        }
    );

    if (uploadId.isEmpty())
    {
        handleFailure(VirtualCameraState::UploadFailed, config.errorMessage);
        return;
    }

    d->state.status = VirtualCameraState::Uploading;
    d->state.currentUpload = appContext()->uploadManager()->state(uploadId);

    emit stateChanged(d->state);
}

VirtualCameraState::Status VirtualCameraWorker::calculateNewStatus(VirtualCameraState::Error error)
{
    switch (error)
    {
    case VirtualCameraState::LockRequestFailed:
    case VirtualCameraState::CameraSnatched:
        return VirtualCameraState::LockedByOtherClient;
    case VirtualCameraState::UploadFailed:
    case VirtualCameraState::ConsumeRequestFailed:
        return VirtualCameraState::Unlocked;
    default:
        return VirtualCameraState::Unlocked;
    }
}

QString VirtualCameraWorker::calculateErrorMessage(VirtualCameraState::Error /*error*/, const QString& errorMessage)
{
    if (!errorMessage.isEmpty())
        return errorMessage;

    // There really is no reason to explain what went wrong in the FSM.
    return tr("Failed to send request to the server.");
}

void VirtualCameraWorker::handleFailure(VirtualCameraState::Error error, const QString& errorMessage)
{
    handleStop();

    d->state.queue.clear();
    d->state.status = calculateNewStatus(error);
    d->state.error = error;

    emit stateChanged(d->state);
    emit this->error(d->state, calculateErrorMessage(error, errorMessage));
    emit finished();
}

void VirtualCameraWorker::handleStop()
{
    d->requests.cancelAllRequests();

    if (!isWorking() || !NX_ASSERT(connection()))
        return;

    if (d->state.status == VirtualCameraState::Uploading)
        appContext()->uploadManager()->cancelUpload(d->state.currentUpload.id);

    connectedServerApi()->releaseVirtualCameraLock(
        d->camera,
        d->lockToken,
        nullptr);
}

void VirtualCameraWorker::handleStatusFinished(bool success, const QnVirtualCameraStatusReply& result)
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
        if (d->state.status != VirtualCameraState::LockedByOtherClient
            || d->state.lockUserId != result.userId)
        {
            d->state.status = VirtualCameraState::LockedByOtherClient;
            d->state.lockUserId = result.userId;

            emit stateChanged(d->state);
        }
    }
    else
    {
        if (d->state.status == VirtualCameraState::LockedByOtherClient)
        {
            d->state.status = VirtualCameraState::Unlocked;
            d->state.lockUserId = QnUuid();

            emit stateChanged(d->state);
        }
    }
}

void VirtualCameraWorker::handleLockFinished(bool success, const QnVirtualCameraStatusReply& result)
{
    if (!success)
    {
        handleFailure(VirtualCameraState::LockRequestFailed);
        return;
    }

    // Already working => that's a stray reply, ignore it.
    if (isWorking())
        return;

    if (!result.success)
    {
        d->state.lockUserId = result.userId;
        handleFailure(VirtualCameraState::CameraSnatched);
        return;
    }

    d->state.status = VirtualCameraState::Locked;
    d->lockToken = result.token;

    emit stateChanged(d->state);
    processCurrentFile();
}

void VirtualCameraWorker::handleExtendFinished(bool success, const QnVirtualCameraStatusReply& result)
{
    // Errors are safe to ignore here as we'll resend the command soon anyway.
    if (!success)
        return;

    // Already done & sorted => that's a stray reply, ignore it.
    if (!isWorking())
        return;

    if (!result.success)
    {
        handleFailure(VirtualCameraState::CameraSnatched);
        return;
    }

    if(d->state.status == VirtualCameraState::Consuming)
    {
        d->state.consumeProgress = result.consuming ? std::min(99, result.progress) : 100;
        emit stateChanged(d->state);

        if (!result.consuming)
            processNextFile();
    }
}

void VirtualCameraWorker::handleUnlockFinished(bool /*success*/, const QnVirtualCameraStatusReply& /*result*/)
{
    // Not working => that's a stray reply, ignore it.
    if (!isWorking())
        return;

    // User has started another upload while we were waiting for a reply, ignore this one.
    if (!d->state.isDone())
        return;

    d->state.status = VirtualCameraState::Unlocked;

    emit stateChanged(d->state);

    d->state.queue.clear();
    d->state.currentIndex = 0;

    emit finished();
}

void VirtualCameraWorker::handleUploadProgress(const UploadState& state)
{
    // In theory we should never get here in non-working state, but we want to feel safe.
    if (!isWorking() || !NX_ASSERT(connection()))
        return;

    d->state.currentUpload = state;

    if (state.status == UploadState::Error)
    {
        handleFailure(VirtualCameraState::UploadFailed, state.errorMessage);
        return;
    }

    if (state.status == UploadState::Done)
    {
        d->state.status = VirtualCameraState::Consuming;
        d->state.consumeProgress = 0;

        const auto callback = nx::utils::guarded(this,
            [this](bool success, rest::Handle handle, const rest::ServerConnection::EmptyResponseType&)
            {
                d->requests.releaseHandle(handle);
                handleConsumeStarted(success);
            });

        d->requests.storeHandle(connectedServerApi()->consumeVirtualCameraFile(
            d->camera,
            d->lockToken,
            d->state.currentUpload.destination,
            d->state.currentFile().startTimeMs,
            callback,
            thread()));
    }

    emit stateChanged(d->state);
}

void VirtualCameraWorker::handleConsumeStarted(bool success)
{
    if (!success)
        handleFailure(VirtualCameraState::ConsumeRequestFailed);

    // Do nothing here, polling is done in timer event.
}

void VirtualCameraWorker::pollExtend()
{
    // Not working => no reason to poll.
    if (!isWorking() || !NX_ASSERT(connection()))
        return;

    const auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
        {
            d->requests.releaseHandle(handle);
            QnVirtualCameraStatusReply reply = result.deserialized<QnVirtualCameraStatusReply>();
            handleExtendFinished(success, reply);
        });
    d->requests.storeHandle(connectedServerApi()->extendVirtualCameraLock(
        d->camera,
        d->user,
        d->lockToken,
        kLockTtlMSec,
        callback,
        thread()));
}

} // namespace nx::vms::client::desktop
